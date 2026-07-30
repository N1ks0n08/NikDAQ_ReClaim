#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <algorithm>

struct Order {
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    char side;

    Order* prev = nullptr;
    Order* next = nullptr;;
};

struct Fill {
    uint64_t aggressive_order_id;
    uint64_t passive_order_id;
    uint32_t price;
    uint32_t quantity;
};

struct AddResult {
    bool added = false;
    std::vector<Fill> fills;
};

struct ReplaceResult {
    bool canceled;
    AddResult add_result;
};

struct OrderPool {
    Order* free_head = nullptr;
    std::vector<Order> orderpool;

    OrderPool(size_t pool_size = 2000000) {
        orderpool.resize(pool_size);

        for (size_t i = 0; i < pool_size - 1; ++i) {
            orderpool[i].next = &orderpool[i + 1];
        }
        free_head = &orderpool[0];
        orderpool[pool_size - 1].next = nullptr;
    }

    Order* allocate() {
        if (free_head == nullptr) {
            return nullptr;
        } else {
            Order* free_slot = free_head;
            free_head = free_head->next;
            return free_slot;
        }
    }

    void deallocate(Order* o) {
        o->next = free_head;
        free_head = o;
    }
};

struct PriceLevel {
    Order* head = nullptr;
    Order* tail = nullptr;

    void push_back(Order* o) {
        o->next = nullptr;
        // PriceLevel is completely empty:
        if (tail == nullptr) {
            head = o;
            tail = o;
            
            head->prev = nullptr;
            tail->next = nullptr;
        } else {
        // Price Level is NOT completely empty:
            tail->next = o;
            o->prev = tail;
            tail = o;
        }

    }

    void unlink(Order* o) {
        // if it's the ONLY element in PriceLevel
        if (o == head && o == tail) {
            head = nullptr;
            tail = nullptr;
        }
        // if it's the HEAD
        else if (o == head) {
            head = o->next;
            head->prev = nullptr;
            o->next = nullptr;
        }
        // if it's the TAIL
        else if (o == tail) {
            tail = o->prev;
            tail->next = nullptr;
            o->prev = nullptr;
        }
        // if it's in between
        else {
            o->prev->next = o->next;
            o->next->prev = o->prev;
            o->prev = nullptr;
            o->next = nullptr;
        }
    }
};

struct PriceLevelArrayManager {
    std::vector<PriceLevel> price_level_array;
    uint32_t base_price;

    PriceLevelArrayManager(uint32_t price) {
        base_price = price;
        price_level_array.resize(1601);
    }

    bool in_range(uint32_t price) {
        return (price >= (base_price - 800)) && (price <= (base_price + 800));
    }

    PriceLevel* get_price_level(uint32_t price) {
        if (in_range(price)) {
            return &price_level_array[(price + 800) - base_price];
        } else {
            return nullptr;
        }
    }

    PriceLevel* best_bid() {
        for (size_t i = price_level_array.size() - 1; i-- > 0; ) {
            if (price_level_array[i].head != nullptr) {
                if (price_level_array[i].head->side == 'B') {
                    return &price_level_array[i];
                }
            }
        }

        return nullptr;
    }

    PriceLevel* best_ask() {
        for (size_t i = 0; i < price_level_array.size(); ++i) {
            if (price_level_array[i].head != nullptr) {
                if (price_level_array[i].head->side == 'S') {
                    return &price_level_array[i];
                }
            }
        }

        return nullptr;
    }
};

struct OrderBook {
    std::string asset_symbol;
    OrderPool orderpool_manager;
    PriceLevelArrayManager price_level_array_manager;
    std::unordered_map<uint64_t, Order*> order_id_map;

    OrderBook(std::string symbol, uint32_t price) 
    : price_level_array_manager(price) {
        asset_symbol = symbol;
    }

    std::vector<Fill> match(Order* order) {
        std::vector<Fill> fills;

        // INCOMING BUY ORDER
        if (order->side == 'B') {
            PriceLevel* current_price_level = price_level_array_manager.best_ask();
            if (current_price_level != nullptr) {
                while (order->price >= current_price_level->head->price) {
                    Fill fill;
                    fill.aggressive_order_id = order->order_id;
                    fill.passive_order_id = current_price_level->head->order_id;
                    fill.price = current_price_level->head->price;
                    fill.quantity = std::min(order->quantity, current_price_level->head->quantity);
                    fills.emplace_back(fill);

                    order->quantity -= fill.quantity;
                    current_price_level->head->quantity -= fill.quantity;

                    if (current_price_level->head->quantity == 0) {
                        Order* filled_resting_order = current_price_level->head;
                        order_id_map.erase(filled_resting_order->order_id);
                        current_price_level->unlink(filled_resting_order);
                        orderpool_manager.deallocate(filled_resting_order);
                    }

                    if (current_price_level->head == nullptr) {
                        current_price_level = price_level_array_manager.best_ask();
                    }

                    if (order->quantity == 0) {
                        return fills;
                    }
                    if (current_price_level == nullptr) return fills;
                }

                return fills;
            } else {
                return fills;
            }
        }
        else {
        // INCOMING SELL ORDER
            PriceLevel* current_price_level = price_level_array_manager.best_bid();
            if (current_price_level != nullptr) {
                while (order->price <= current_price_level->head->price) {
                    Fill fill;
                    fill.aggressive_order_id = order->order_id;
                    fill.passive_order_id = current_price_level->head->order_id;
                    fill.price = current_price_level->head->price;
                    fill.quantity = std::min(order->quantity, current_price_level->head->quantity);
                    fills.emplace_back(fill);

                    order->quantity -= fill.quantity;
                    current_price_level->head->quantity -= fill.quantity;

                    if (current_price_level->head->quantity == 0) {
                        Order* filled_resting_order = current_price_level->head;
                        order_id_map.erase(filled_resting_order->order_id);
                        current_price_level->unlink(filled_resting_order);
                        orderpool_manager.deallocate(filled_resting_order);
                    }

                    if (current_price_level->head == nullptr) {
                        current_price_level = price_level_array_manager.best_bid();
                    }

                    if (order->quantity == 0) {
                        return fills;
                    }

                    if (current_price_level == nullptr) return fills;
                }

                return fills;
            } else {
                return fills;
            }
        }
    }

    AddResult add(Order order) {
        AddResult add_result;
        add_result.fills = match(&order);
        if (price_level_array_manager.get_price_level(order.price) != nullptr) {
            if (order.quantity != 0) {
                order_id_map[order.order_id] = orderpool_manager.allocate();
                *order_id_map[order.order_id] = order;
                price_level_array_manager.get_price_level(order.price)->push_back(order_id_map[order.order_id]);
                add_result.added = true;
            } else {
                add_result.added = false;
            }
        } else {
            add_result.added = false;
        }
        return add_result;
    }

    bool cancel(uint64_t order_id) {
        if (order_id_map.count(order_id) != 0) {
            Order* canceled_order = order_id_map[order_id];
            price_level_array_manager.get_price_level(canceled_order->price)->unlink(canceled_order);
            order_id_map.erase(order_id);
            orderpool_manager.deallocate(canceled_order);

            return true;
        } else {
            return false;
        }
    }

    ReplaceResult replace(uint64_t old_order_id, Order new_order) {
        ReplaceResult replace_result;
        replace_result.canceled = cancel(old_order_id);
        if (replace_result.canceled) {
            replace_result.add_result = add(new_order);
        }

        return replace_result;
    }
};