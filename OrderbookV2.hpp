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

// utilize 1 large vector, with the base price as the midpoint
// 800 price levels below the base price, and 800 price levels above the base price ($8 below, $8 above)
// therefore total array size of 1601
// only for base prices at or above $75
// floor: PriceLevels[0] (0-799: loewr $8 range)
// midpoint/base price: PriceLevels[800]
// ceiling: PriceLevels[1600] (801-1600: upper $8 range)
// the math for indexing thoruhg pirce levels:
// index = 800 - (base price - current price)
// or, since the price is unsigned: 
// index = (current price + 800) - base price
// (current price + 800 ensures no negative numbers)
// NOTE: $75 = 7500 in uint32_t format (7500 cents, 1 tick is 1 cent)
struct PriceLevelArray {
    std::vector<PriceLevel> PriceLevels;
    uint32_t base_price;

    PriceLevelArray(uint32_t price) {
        if (price < 7500) {
            std::cout << "Error: price has to be $75 or above!";
            return;
        }

        PriceLevels.resize(1601);
        base_price = price;
    }

    bool in_range(uint32_t price) {
        return (price >= (base_price - 800)) && (price <= (base_price + 800));
    }

    PriceLevel* get_price_level (uint32_t price) {
        if (in_range(price)) {
            return &PriceLevels[(price + 800) - base_price];
        }

        std::cout << "Error: Price out of range!\n";
        return nullptr;
    }

    // return a pointer to the best bid (null if none)
    // iterate from top to bottom (highest possible BUY order)
    PriceLevel* best_bid() {
        for (size_t i = PriceLevels.size(); i-- > 0; ) {
            if (PriceLevels[i].head != nullptr && PriceLevels[i].head->side == 'B') {
                return &PriceLevels[i];
            }
        }

        return nullptr;
    }

    // return a pointer to the best ask (null if none)
    PriceLevel* best_ask() {
        for (size_t i = 0; i < PriceLevels.size(); ++i) {
            if (PriceLevels[i].head != nullptr && PriceLevels[i].head->side == 'S') {
                return &PriceLevels[i];
            }
        }

        return nullptr;
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