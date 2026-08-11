#include "spsc_queue.hpp"
#include <thread>
#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
    constexpr std::size_t N = 100'000'000;   // big enough for a stable timing
    SPSCQueue<int, 1024> q;

    auto start = std::chrono::steady_clock::now();

    std::thread producer([&]{
        for (std::size_t i = 0; i < N; ) {
            if (q.try_push(static_cast<int>(i))) ++i;
        }
    });

    std::uint64_t checksum = 0;   // consume the popped value — see note below
    std::thread consumer([&]{
        int v;
        for (std::size_t i = 0; i < N; ) {
            if (q.try_pop(v)) { checksum += static_cast<std::uint64_t>(v); ++i; }
        }
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(end - start).count();

    std::cout << "N=" << N
              << "  time=" << secs << "s"
              << "  throughput=" << (N / secs / 1e6) << " M ops/s"
              << "  amortized=" << (secs * 1e9 / N) << " ns/op\n";
    std::cout << "checksum=" << checksum << "\n";
    return 0;
}