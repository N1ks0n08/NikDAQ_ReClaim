#include <array>
#include <cstddef>
#include <atomic>

template <typename T, std::size_t Capacity>
class SPSCQueue {
public:
    bool try_push(const T& item) {
        // going with the slot sacrifice approach
        // if slot is full, tail collides with head on next push
        std::size_t t = tail_.load(std::memory_order_relaxed);
        std::size_t h = headCache_;
        if ((t + 1) % Capacity != h) {
            buffer_[t] = item;
            tail_.store((t + 1) % Capacity, std::memory_order_release); // release; ready to read
            return true;
        } else {
            headCache_ = head_.load(std::memory_order_acquire);
            if ((t + 1) % Capacity != headCache_) {
                buffer_[t] = item;
                tail_.store((t + 1) % Capacity, std::memory_order_release); // release; ready to read
                return true;
            } else {
                return false;
            }
        }
        return false;
    }

    bool try_pop(T& out) {
        // if head == tail, it's empty
        std::size_t t = tailCache_;
        std::size_t h = head_.load(std::memory_order_relaxed);
        if (h != t) {
            out = buffer_[h];
            head_.store((h + 1) % Capacity, std::memory_order_release); // release; advance head and free up slot for producer
            return true;
        } else {
            tailCache_ = tail_.load(std::memory_order_acquire);
            if (h != tailCache_) {
                out = buffer_[h];
                head_.store((h + 1) % Capacity, std::memory_order_release); // release; advance head and free up slot for producer
                return true;
            } else {
                return false;
            }
        }
        return false;
    }

private:
    std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<std::size_t> head_ = 0; // consumer reads from here
    std::size_t tailCache_ = 0;
    alignas(64) std::atomic<std::size_t> tail_ = 0; // producer writes to here
    std::size_t headCache_ = 0;
};
