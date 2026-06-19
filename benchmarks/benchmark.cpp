/**
 * benchmark.cpp — throughput of SPSCRingBuffer across two threads.
 *
 * Compile:
 *   g++ -std=c++20 -O2 -I../include benchmark.cpp -o benchmark
 * Run:
 *   ./benchmark
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>

#include "spsc_ring_buffer.hpp"

int main() {
    constexpr int kCount = 50'000'000;
    rb::SPSCRingBuffer<int, 1024> q;

    auto start = std::chrono::high_resolution_clock::now();

    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i)
            while (!q.push(i)) { /* spin */ }
    });

    int64_t sum = 0;
    std::thread consumer([&] {
        int v;
        for (int i = 0; i < kCount; ++i) {
            while (!q.pop(v)) { /* spin */ }
            sum += v;
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    auto ns  = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    const double seconds   = ns / 1e9;
    const double msgs_per_s = kCount / seconds;
    const double ns_per_msg = static_cast<double>(ns) / kCount;

    std::cout << "Messages:        " << kCount               << "\n";
    std::cout << "Total time:      " << seconds      << " s\n";
    std::cout << "Throughput:      " << msgs_per_s / 1e6 << " M msg/s\n";
    std::cout << "Latency / msg:   " << ns_per_msg   << " ns\n";
    std::cout << "Checksum:        " << sum                  << "\n";

    return 0;
}
