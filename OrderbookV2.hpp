#pragma once
#include <map>
#include <deque>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <iostream>

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
            std::cout << "ERROR: Orderpool limit exceeded!\n";
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
    std::string asset_symbol;                               // name of the asset being traded
    OrderPool orderpool;                                    // pre-allocated order pool
    PriceLevelArray price_level_array;                      // pirce level array for the specific asset
    std::unordered_map<uint64_t, Order*> order_index_map;   // allows for mutating/accessing order via order id in the memory pool

    // generate an orderbook depending on the asset symbol and price given
    OrderBook(std::string symbol, uint32_t base_price)
    : price_level_array(base_price) {
        asset_symbol = symbol;
    };

    // match() returns a vector of Fill's that may happen when matching incoming orders against the book
    std::vector<Fill> match(Order* o) {
        std::vector<Fill> results;

        // BUY ORDER
        if (o->side == 'B') {
            // while opposing orders exist, match it there
            // else, return
            PriceLevel* current_price_level = price_level_array.best_ask();
            if (current_price_level == nullptr) {
                std::cout << "NO CURRENT BEST ASKS EXIST!\n";
                return results;
            } else {
                while (current_price_level != nullptr) {
                    if (o->price >= current_price_level->head->price) {
                        Fill fill;
                        fill.aggressive_order_id = o->order_id;
                        fill.passive_order_id = current_price_level->head->order_id;
                        uint32_t matched_shares = std::min(o->quantity, current_price_level->head->quantity);
                        fill.price = current_price_level->head->price;
                        fill.quantity = matched_shares;
                        o->quantity -= matched_shares;
                        current_price_level->head->quantity -= matched_shares;
                        results.emplace_back(fill);

                        // check if current head order of a price level is filled
                        // then, once it is filled, remove it from the order id map, unlink from the price level, then deallocate it
                        if (current_price_level->head->quantity == 0) {
                            Order* filled = current_price_level->head;
                            order_index_map.erase(filled->order_id);
                            current_price_level->unlink(filled);
                            orderpool.deallocate(filled);
                        }

                        // check if the current price level needs to be updated
                        if (current_price_level->head == nullptr && current_price_level->tail == nullptr) {
                            current_price_level = price_level_array.best_ask();
                        }

                        if (o->quantity == 0) {
                            std::cout << "ORDER FULLY FILLED\n";
                            return results;
                        }
                    } else {
                        return results;
                    }
                }
            }
        }
        // SELL ORDER
        else {
            // while opposing orders exist, match it there
            // else, return
           PriceLevel* current_price_level = price_level_array.best_bid();
            if (current_price_level == nullptr) {
                std::cout << "NO CURRENT BEST BIDS EXIST!\n";
                return results;
            } else {
                while (current_price_level != nullptr) {
                    if (o->price <= current_price_level->head->price) {
                        Fill fill;
                        fill.aggressive_order_id = o->order_id;
                        fill.passive_order_id = current_price_level->head->order_id;
                        uint32_t matched_shares = std::min(o->quantity, current_price_level->head->quantity);
                        fill.price = current_price_level->head->price;
                        fill.quantity = matched_shares;
                        o->quantity -= matched_shares;
                        current_price_level->head->quantity -= matched_shares;
                        results.emplace_back(fill);

                        // check if current head order of a price level is filled
                        // then, once it is filled, remove it from the order id map, unlink from the price level, then deallocate it
                        if (current_price_level->head->quantity == 0) {
                            Order* filled = current_price_level->head;
                            order_index_map.erase(filled->order_id);
                            current_price_level->unlink(filled);
                            orderpool.deallocate(filled);
                        }

                        // check if the current price level needs to be updated
                        if (current_price_level->head == nullptr && current_price_level->tail == nullptr) {
                            current_price_level = price_level_array.best_bid();
                        }

                        if (o->quantity == 0) {
                            std::cout << "ORDER FULLY FILLED\n";
                            return results;
                        }
                    } else {
                        return results;
                    }
                }
            }
        }
        return results;
    }

    // add() takes an Order object and returns a vector of Fill's from calling match()
    // and ultimately adds or removes the order from the book utilizing the memory pool
    std::vector<Fill> add(Order o) {
        std::vector<Fill> results = match(&o);

        // if the order is still not filled, add it to the orderbook
        // allocate from the orderpool first, then create an order id -> Order* entry, then add it to it's corresponding price level
        if (o.quantity != 0) {
            Order* slot = orderpool.allocate();
            order_index_map[o.order_id] = slot;
            *slot = o; // fill slot with the actual order details
            price_level_array.get_price_level(o.price)->push_back(order_index_map[o.order_id]);
            std::cout << "ORDER ADDED TO ORDERBOOK\n";
        } else {
            std::cout << "ORDER NOT ADDED: FILLED UPON MATCH\n";
        }
        return results;
    }

    // cancel() returns a bool letting the caller knonw if the order was successfully canceled
    // or fialed to do so (due to various reasons such as order id bieng invalid or order no longer existing)
    bool cancel(uint64_t order_id) {
        if (order_index_map.count(order_id)) {
            price_level_array.get_price_level(order_index_map[order_id]->price)->unlink(order_index_map[order_id]);
            orderpool.deallocate(order_index_map[order_id]);
            order_index_map.erase(order_id);
            std::cout << "ORDER SUCCESSFULLY CANCELED\n";
            return true;
        }
        std::cout << "FAILED TO CANCEL ORDER\n";
        return false;
    }


    // replace() returns a ReplaceResult that combines the results from cancel() + add() (in this order)
    ReplaceResult replace(uint64_t old_order_id, Order new_order) {
        ReplaceResult result;

        result.replaced = cancel(old_order_id);
        if (result.replaced) {
            result.fills = add(new_order);
        }

        return result;
    };
};