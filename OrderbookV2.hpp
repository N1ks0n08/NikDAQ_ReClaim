#pragma once
#include <map>
#include <deque>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstddef>

struct Order {
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    char side;
    // intrusive DLL portions:
    Order* prev = nullptr;
    Order* next = nullptr;
};

struct OrderPool {
    std::vector<Order> pool;
    Order* free_head = nullptr;

    // construct the pool on object creation
    OrderPool(size_t n = 4000000) {
        if (n == 0) return;  // empty pool: free_head stays nullptr, allocate() returns null
        pool.resize(n); // populate and fault vector with slots
        // link all orders (except the end) to the next order initially of the "free list"
        for (size_t i = 0; i < pool.size() - 1; ++i) {
            pool[i].next = &pool[i + 1];
        }
        pool[pool.size() - 1].next = nullptr; // end of vector order has nothing past it

        free_head = &pool[0]; // first empty slot is the free head
    }

    // allocate() returns a free order from the free order list
    Order* allocate() {
        if (free_head == nullptr) {
            return nullptr;
        }

        Order* free_order_slot = free_head;
        free_head = free_head->next;
        return free_order_slot;
    }

    // dellocate() adds used order back to the free order list
    void deallocate(Order* o) {
        o->next = free_head;
        free_head = o;
    }
};

struct PriceLevel {
    Order* head = nullptr; // orders closer to this are old orders (earlier in time priority)
    Order* tail = nullptr; // orders closer to this for new orders (later in time priority)

    // push_back() adds an order to the end of the FIFO DLL
    void push_back(Order* o) {
        // current order is last node; nothing after it
        o->next = nullptr;
        // current order is last node; prevous tale is now before the current order
        o->prev = tail;
        // check if PriceLevel is empty or not before adding new order:
        if (tail == nullptr) {
            // tail == nullptr implies head was also empty; therefore empty PriceLevel
            // since o is the only order, it is boht the "head" and "tail"
            head = o;
        } else {
            // if PriceLevel is NOT empty,
            // simply link tail to the current order
            tail->next = o;
        }
        // now the tail is the current orrder
        tail = o;
    }

    // unlink() removes an order from the FIFO DLL
    void unlink(Order* o) {
        // handle order if it's the only element
        if (o == head && o == tail) {
            head = nullptr;
            tail = nullptr;
        }
        // handle order if it's the head
        else if (o == head) {
            head = o->next; // set head as the next order in front of the old head
            head->prev = nullptr; // ensure the new head has no orders behind it
        }
        // handle order if it's the tail
        else if (o == tail) {
            tail = o->prev; // make the lement before the new last node
            tail->next = nullptr; // ensure the current tail node has nothing past it
        }
        // handle order if neither
        else {
            o->prev->next = o->next; // link previous element to the next element
            o->next->prev = o->prev; // link the next elememnt to the previous element
        }
    }
};

struct Fill {
    uint64_t aggressive_order_id;
    uint64_t passive_order_id;
    uint32_t price;
    uint32_t quantity;
};

struct ReplaceResult {
    bool replaced = false;
    std::vector<Fill> fills;
};

struct OrderBook {
    
};