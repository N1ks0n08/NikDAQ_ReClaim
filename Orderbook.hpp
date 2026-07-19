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
    std::map<uint32_t, std::deque<Order>> asks;                         // ascending;  best ask  = begin()
    std::map<uint32_t, std::deque<Order>, std::greater<uint32_t>> bids; // descending; best bid = begin()

    // match() returns a vector; zero, one or many fills from a single match
    // match() runs until no more orders can be matched from the add() function
    // match() doesn't own the order because it only attempts to fill the order
    std::vector<Fill> match(Order& order) {
        std::vector<Fill> fills;
        // Do these in order:
        // 1. if opposite side is empty or doesn't cross, don't match
        // 2. if cross is found, then iteratively match 
        //    until the aggressor is matched fully
        if (order.side == 'B') {
            // Handle matching an incoming BUY order
            // EDGE CASE: nothing to match against OR order doesn't cross
            if (asks.empty() || order.price < asks.begin()->first) {
                return fills;
            }

            while (order.quantity != 0 && !asks.empty()) {
                auto current_pricelevel = asks.begin();
                const auto& current_price = current_pricelevel->first;
                auto& order_deque = current_pricelevel->second;

                // check if the resting liquidity is within price
                if (current_price > order.price) break; // TOO HIGH OF A PRICE

                // matching the orders
                Order& resting = order_deque.front();
                auto fill_quantity = std::min(order.quantity, resting.quantity);
                order.quantity -= fill_quantity;
                resting.quantity -= fill_quantity;

                // generate fill report
                fills.emplace_back(order.order_id, resting.order_id, current_price, fill_quantity);
                
                // if resting order is filled, delete it from the orderbook
                if (resting.quantity == 0) {
                    order_deque.pop_front();
                }
                // if the price level is empty, delete it from the orderbook
                if (order_deque.empty()) {
                    asks.erase(current_pricelevel);
                }
            }
        } else {
            // Handle matching an incoming SELL order
            if (bids.empty() || order.price > bids.begin()->first) {
                return fills;
            }

            while (order.quantity != 0 && !bids.empty()) {
                auto current_pricelevel = bids.begin();
                const auto& current_price = current_pricelevel->first;
                auto& order_deque = current_pricelevel->second;

                // check if the resting liquidity is within price
                if (current_price < order.price) break; // TOO LOW OF A PRICE

                // matching the orders
                Order& resting = order_deque.front();
                auto fill_quantity = std::min(order.quantity, resting.quantity);
                order.quantity -= fill_quantity;
                resting.quantity -= fill_quantity;

                // generate fill report
                fills.emplace_back(order.order_id, resting.order_id, current_price, fill_quantity);
                
                // if resting order is filled, delete it from the orderbook
                if (resting.quantity == 0) {
                    order_deque.pop_front();
                }
                // if theh price level is empty, delete it from the orderbook
                if (order_deque.empty()) {
                    bids.erase(current_pricelevel);
                }
            }
        }

        return fills;
    }

    // add() returns a vector; same as match()
    // add() handles remaining aggressive order shares, if any
    // add() owns the order since it adds the order to the book if possible
    std::vector<Fill> add(Order order) {
        // send order to be matched first
        std::vector<Fill> result = match(order);
        // if the order still has quantity left, add to the book
        if (order.quantity != 0) {
            if (order.side == 'B') {
                bids[order.price].push_back(order);
            } else {
                asks[order.price].push_back(order);
            }
        }
        // return the fills
        return result;
    }

    // cancel() returns a bool; tells if the order was cancelled successfully
    // or if it was invalid (due to order being already filled, invalid id, etc)
    bool cancel(uint64_t cancel_order_id) {
        // currently the naive version; will have to scan the
        // entire bids/asks sides to find the order to cancel (if found)

        // iterate through the price levels of the bids
        for (auto it = bids.begin(); it != bids.end(); ++it) {
            // iterate through order deques of each price level
            for (auto deque_it = it->second.begin(); deque_it != it->second.end(); ++deque_it) {
                if (deque_it->order_id == cancel_order_id) {
                    it->second.erase(deque_it);
                    
                    // chcek if the price level is empty after deleting the order
                    if (it->second.empty()) bids.erase(it);
                    return true;
                }
            }
        }

        // iterate through the price levels of the asks
        for (auto it = asks.begin(); it != asks.end(); ++it) {
            // iterate through order deques of each price level
            for (auto deque_it = it->second.begin(); deque_it != it->second.end(); ++deque_it) {
                if (deque_it->order_id == cancel_order_id) {
                    it->second.erase(deque_it);

                    // chcek if the price level is empty after deleting the order
                    if (it->second.empty()) asks.erase(it);
                    return true;
                }
            }
        }

        return false;
    }

    // replace() returns ReplaceResult to represent status on:
    // 1. if the order was cancelled successfully
    // 2. if the order that was added also had fills or not
    ReplaceResult replace(uint64_t old_order_id, Order new_order) {
        // replace is basically a cancel() then an add()
        ReplaceResult result;
        result.replaced = cancel(old_order_id);
        if (result.replaced) {
            result.fills = add(new_order);
        }

        return result;
    }

};