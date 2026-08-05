#include <array>
#include <cstddef>
#include <iostream>
#include <atomic>
#include <thread>
#include <vector>

template <typename T, std::size_t Capacity>
class SPSCQueue {
public:
    bool try_push(const T& item) {
        // going with the slot sacrifice approach
        // if slot is full, tail collides with head on next push
        std::size_t t = tail_.load(std::memory_order_relaxed);
        std::size_t h = head_.load(std::memory_order_acquire);
        if ((t + 1) % Capacity != h) {
            buffer_[t] = item;
            tail_.store((t + 1) % Capacity, std::memory_order_release); // release; ready to read
            return true;
        }
        return false;
    }

    bool try_pop(T& out) {
        // if head == tail, it's empty
        std::size_t t = tail_.load(std::memory_order_acquire);
        std::size_t h = head_.load(std::memory_order_relaxed);
        if (h != t) {
            out = buffer_[h];
            head_.store((h + 1) % Capacity, std::memory_order_release); // release; advance head and free up slot for producer
            return true;
        }
        return false;
    }

private:
    std::array<T, Capacity> buffer_;
    std::atomic<std::size_t> head_ = 0; // consumer reads from here
    std::atomic<std::size_t> tail_ = 0; // producer writes to here
};


#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAILED: " #cond "  (line " << __LINE__ << ")\n"; \
    std::abort(); } } while (0)



int main() {
    SPSCQueue<int, 128> queue;
    std::vector<int> result;
    result.resize(100000);
    // producer:
    std::thread t1([&] {
        std::size_t i = 0;
        while (i < result.size()) {
            if (queue.try_push(static_cast<int>(i))) {
                ++i;
            }
        }
    });
    // consumer:
    std::thread t2([&] {
        std::size_t i = 0;
        while (i != result.size()) {
            if (queue.try_pop(result[i])) {
                ++i;
            }
        }
    });

    t1.join();
    t2.join();

    for (std::size_t k = 0; k < result.size(); ++k) {
        CHECK(result[k] == static_cast<int>(k));
    }
    std::cout << "threaded test passed\n";

    return 0;
}
