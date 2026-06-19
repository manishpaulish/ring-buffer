#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <new>
#include <type_traits>

namespace rb {

constexpr bool is_power_of_2_spsc(std::size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * SPSCRingBuffer<T, N>
 *
 * Lock-free single-producer / single-consumer ring buffer.
 *
 * Exactly one thread calls push() and exactly one (different) thread calls
 * pop(). No locks, no mutexes. Correctness relies on:
 *
 *   - head and tail being atomic
 *   - acquire/release memory ordering so the consumer sees a fully written
 *     slot before it sees the updated tail, and vice versa
 *   - head and tail living on separate cache lines (alignas) to avoid false
 *     sharing — otherwise the producer's write to tail would invalidate the
 *     consumer's cache line holding head on every operation
 *
 * N must be a power of two. Usable capacity is N - 1.
 *
 * T must be trivially copyable: the buffer is copied by plain assignment and
 * a non-trivial type could observe a half-constructed object across threads.
 */
template<typename T, std::size_t N>
class SPSCRingBuffer {
    static_assert(is_power_of_2_spsc(N), "SPSCRingBuffer<T, N>: N must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SPSCRingBuffer<T, N>: T must be trivially copyable");

    // Cache-line size. 64 on x86-64, 128 on Apple Silicon. 64 is a safe,
    // portable default for the padding here.
    static constexpr std::size_t kCacheLine = 64;

    std::array<T, N> buffer_{};

    // head and tail on separate cache lines — prevents false sharing between
    // the producer thread (writes tail) and consumer thread (writes head).
    alignas(kCacheLine) std::atomic<std::size_t> head_{0};
    alignas(kCacheLine) std::atomic<std::size_t> tail_{0};

public:
    /**
     * Producer side. Push one item. Returns false if full. O(1).
     * Called by the producer thread only.
     */
    bool push(const T& item) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t next = (tail + 1) & (N - 1);

        // acquire: ensure we see the consumer's latest head update
        if (next == head_.load(std::memory_order_acquire)) {
            return false;  // full
        }

        buffer_[tail] = item;

        // release: publish the slot write before advancing tail so the
        // consumer cannot read tail and then see a stale slot
        tail_.store(next, std::memory_order_release);
        return true;
    }

    /**
     * Consumer side. Pop one item into `out`. Returns false if empty. O(1).
     * Called by the consumer thread only.
     */
    bool pop(T& out) {
        const std::size_t head = head_.load(std::memory_order_relaxed);

        // acquire: ensure we see the producer's latest tail update
        if (head == tail_.load(std::memory_order_acquire)) {
            return false;  // empty
        }

        out = buffer_[head];

        // release: publish that the slot is free before advancing head
        head_.store((head + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    static constexpr std::size_t capacity() { return N; }
};

} // namespace rb
