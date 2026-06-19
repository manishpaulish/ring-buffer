# ring-buffer

Header-only C++ ring buffer with compile-time constraints and a lock-free single-producer / single-consumer (SPSC) variant.

---

## What's here

| Header | Description |
|---|---|
| `include/ring_buffer.hpp` | Single-threaded ring buffer. Fixed capacity, stack-allocated, O(1) push/pop. |
| `include/spsc_ring_buffer.hpp` | Lock-free SPSC ring buffer. One producer thread, one consumer thread, no locks. |

Both are header-only — just include and go. C++20.

---

## Design

### Power-of-two capacity → bitmask wraparound

`N` must be a power of two. This lets index wrapping use a bitmask instead of a modulo:

```cpp
next = (index + 1) & (N - 1);   // fast — single AND instruction
// vs
next = (index + 1) % N;         // slow — division
```

Enforced at compile time:

```cpp
static_assert(is_power_of_2(N), "N must be a power of 2");
```

### One empty slot

To tell "full" from "empty" without an extra counter, one slot is always left unused:

```
empty: head == tail
full:  (tail + 1) == head
```

A buffer of `N` slots therefore holds `N - 1` items.

### Trivially-copyable element type

Items are moved by plain assignment / byte copy, so `T` must be trivially copyable. Enforced at compile time:

```cpp
static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
```

---

## Lock-free SPSC variant

`SPSCRingBuffer` allows exactly one producer thread and one consumer thread to communicate with no locks. Two things make it correct and fast:

**Acquire/release ordering.** The producer writes the slot, then publishes the new `tail` with `memory_order_release`. The consumer reads `tail` with `memory_order_acquire`, guaranteeing it never sees an advanced `tail` before the slot data it points to. The reverse holds for `head`.

**Cache-line separation.** `head` and `tail` are placed on separate cache lines with `alignas`:

```cpp
alignas(64) std::atomic<std::size_t> head_;
alignas(64) std::atomic<std::size_t> tail_;
```

Without this, the producer's write to `tail` and the consumer's write to `head` would land on the same cache line. Every write by one thread would invalidate the other's cache line — "false sharing" — and throughput would collapse.

---

## Usage

```cpp
#include "ring_buffer.hpp"

rb::RingBuffer<int, 1024> q;     // 1024 slots, holds 1023 items

q.push(42);
int v;
q.pop(v);                        // v == 42
```

```cpp
#include "spsc_ring_buffer.hpp"

rb::SPSCRingBuffer<int, 1024> q;

// producer thread:
while (!q.push(value)) { /* spin until space */ }

// consumer thread:
int v;
while (!q.pop(v)) { /* spin until data */ }
```

---

## Build and test

```bash
# tests
g++ -std=c++20 -O2 -pthread -Iinclude tests/tests.cpp -o run_tests
./run_tests

# benchmark
g++ -std=c++20 -O2 -pthread -Iinclude benchmarks/benchmark.cpp -o run_bench
./run_bench
```

Tests cover: basic push/pop, index wraparound past N, struct element types, and a two-thread SPSC test moving 1,000,000 integers with a checksum.

---

## Benchmark

`benchmark.cpp` moves 50,000,000 integers through the SPSC buffer between two threads.

```
Messages:      50,000,000
Throughput:    32.6 M msg/s
Latency / msg: 30.7 ns
```


---

## Project structure

```
ring-buffer/
├── include/
│   ├── ring_buffer.hpp        — single-threaded ring buffer
│   └── spsc_ring_buffer.hpp   — lock-free SPSC ring buffer
├── tests/
│   └── tests.cpp              — correctness tests (incl. threaded SPSC)
├── benchmarks/
│   └── benchmark.cpp          — two-thread throughput benchmark
└── README.md
```
