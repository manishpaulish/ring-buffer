#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

namespace rb {

/**
 * Compile-time check: is N a power of two?
 * A power of two has exactly one bit set, so n & (n-1) clears it to zero.
 */
constexpr bool is_power_of_2(std::size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * RingBuffer<T, N>
 *
 * Single-producer / single-consumer ring buffer (not thread-safe — see
 * SPSCRingBuffer for the lock-free cross-thread version).
 *
 * Fixed capacity N, stack-allocated, no heap allocation ever.
 * N must be a power of two so index wrapping uses a bitmask (& (N-1))
 * instead of a modulo, which is significantly faster.
 *
 * One slot is always kept empty to distinguish full from empty, so the
 * usable capacity is N - 1.
 *
 * Both constraints are enforced at compile time via static_assert.
 */
template<typename T, std::size_t N>
class RingBuffer {
    static_assert(is_power_of_2(N), "RingBuffer<T, N>: N must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>,
                  "RingBuffer<T, N>: T must be trivially copyable");

    std::array<T, N> buffer_{};
    std::size_t head_ = 0;  // read index
    std::size_t tail_ = 0;  // write index

public:
    /** Push one item. Returns false if the buffer is full. O(1). */
    bool push(const T& item) {
        const std::size_t next = (tail_ + 1) & (N - 1);
        if (next == head_) return false;   // full
        buffer_[tail_] = item;
        tail_ = next;
        return true;
    }

    /** Pop one item into `out`. Returns false if the buffer is empty. O(1). */
    bool pop(T& out) {
        if (head_ == tail_) return false;  // empty
        out = buffer_[head_];
        head_ = (head_ + 1) & (N - 1);
        return true;
    }

    /** Number of items currently stored. O(1). */
    std::size_t size() const {
        return (tail_ - head_) & (N - 1);
    }

    bool empty() const { return head_ == tail_; }
    bool full()  const { return ((tail_ + 1) & (N - 1)) == head_; }

    /** Total slot count N. Usable capacity is N - 1. */
    static constexpr std::size_t capacity() { return N; }
};

} // namespace rb
