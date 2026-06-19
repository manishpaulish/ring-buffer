/**
 * tests.cpp — correctness tests for RingBuffer and SPSCRingBuffer.
 *
 * Compile:
 *   g++ -std=c++20 -O2 -I../include tests.cpp -o tests
 * Run:
 *   ./tests
 */

#include <iostream>
#include <thread>
#include <vector>
#include <cstdint>

#include "ring_buffer.hpp"
#include "spsc_ring_buffer.hpp"

static int g_failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            std::cout << "FAIL: " << #cond                               \
                      << " (line " << __LINE__ << ")\n";                 \
            ++g_failures;                                                \
        }                                                                \
    } while (0)

struct Item {
    int64_t a;
    int32_t b;
    bool operator==(const Item& o) const { return a == o.a && b == o.b; }
};

void test_basic_push_pop() {
    rb::RingBuffer<int, 4> q;       // capacity 4 → holds 3
    CHECK(q.empty());
    CHECK(!q.full());

    CHECK(q.push(1));
    CHECK(q.push(2));
    CHECK(q.push(3));
    CHECK(q.full());
    CHECK(!q.push(4));              // full — rejected

    CHECK(q.size() == 3);

    int v;
    CHECK(q.pop(v) && v == 1);
    CHECK(q.pop(v) && v == 2);
    CHECK(q.pop(v) && v == 3);
    CHECK(!q.pop(v));              // empty
    CHECK(q.empty());
}

void test_wraparound() {
    rb::RingBuffer<int, 4> q;
    int v;
    // Fill, drain, refill many times to force the index to wrap past N.
    for (int round = 0; round < 100; ++round) {
        CHECK(q.push(round * 3 + 0));
        CHECK(q.push(round * 3 + 1));
        CHECK(q.pop(v) && v == round * 3 + 0);
        CHECK(q.push(round * 3 + 2));
        CHECK(q.pop(v) && v == round * 3 + 1);
        CHECK(q.pop(v) && v == round * 3 + 2);
        CHECK(q.empty());
    }
}

void test_struct_items() {
    rb::RingBuffer<Item, 8> q;
    for (int i = 0; i < 7; ++i)
        CHECK(q.push(Item{i * 100LL, i}));
    CHECK(q.full());

    Item out;
    for (int i = 0; i < 7; ++i) {
        CHECK(q.pop(out));
        CHECK(out == (Item{i * 100LL, i}));
    }
    CHECK(q.empty());
}

void test_spsc_single_thread() {
    rb::SPSCRingBuffer<int, 8> q;
    CHECK(q.push(10));
    CHECK(q.push(20));
    int v;
    CHECK(q.pop(v) && v == 10);
    CHECK(q.pop(v) && v == 20);
    CHECK(!q.pop(v));
}

void test_spsc_threaded() {
    constexpr int kCount = 1'000'000;
    rb::SPSCRingBuffer<int, 1024> q;

    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i)
            while (!q.push(i)) { /* spin until space */ }
    });

    int64_t sum = 0;
    std::thread consumer([&] {
        int v;
        for (int i = 0; i < kCount; ++i) {
            while (!q.pop(v)) { /* spin until data */ }
            sum += v;
        }
    });

    producer.join();
    consumer.join();

    // 0 + 1 + ... + (kCount-1) = kCount*(kCount-1)/2
    const int64_t expected = static_cast<int64_t>(kCount) * (kCount - 1) / 2;
    CHECK(sum == expected);
}

int main() {
    test_basic_push_pop();
    test_wraparound();
    test_struct_items();
    test_spsc_single_thread();
    test_spsc_threaded();

    if (g_failures == 0) {
        std::cout << "All tests passed.\n";
        return 0;
    }
    std::cout << g_failures << " test(s) failed.\n";
    return 1;
}
