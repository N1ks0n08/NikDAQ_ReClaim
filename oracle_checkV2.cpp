#include "OrderbookV2.hpp"
#include <iostream>
#include <cassert>

#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAILED: " #cond "  (line " << __LINE__ << ")\n"; \
    std::abort(); } } while (0)

// Helper: fetch the resting order at a given price (or nullptr) via the array,
// so tests can assert on book state without poking internals everywhere.
static Order* head_at(OrderBook& ob, uint32_t price) {
    PriceLevel* lvl = ob.price_level_array.get_price_level(price);
    return lvl ? lvl->head : nullptr;
}

int main() {
    // Base 10000 = $100.00; all prices in cents within +/-800 of base.

    {   // TEST 1: single full fill
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 25, 'S'});
        auto fills = ob.add(Order{2, 10000, 25, 'B'});
        CHECK(fills.size() == 1);
        CHECK(fills[0].price == 10000);
        CHECK(fills[0].aggressive_order_id == 2);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(fills[0].quantity == 25);
        CHECK(ob.price_level_array.best_ask() == nullptr);
        CHECK(ob.price_level_array.best_bid() == nullptr);
    }
    std::cout << "Test 1 (SINGLE FULL FILL): PASS\n";

    {   // TEST 2: partial fill, resting bigger
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 25, 'S'});
        auto fills = ob.add(Order{2, 10000, 10, 'B'});
        CHECK(fills.size() == 1);
        CHECK(fills[0].price == 10000);
        CHECK(fills[0].quantity == 10);
        CHECK(fills[0].passive_order_id == 1);
        Order* r = head_at(ob, 10000);
        CHECK(r != nullptr && r->quantity == 15);
        CHECK(ob.price_level_array.best_bid() == nullptr);
    }
    std::cout << "Test 2 (PARTIAL - RESTING BIGGER): PASS\n";

    {   // TEST 3: partial fill, aggressor bigger
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        auto fills = ob.add(Order{2, 10000, 25, 'B'});
        CHECK(fills.size() == 1);
        CHECK(fills[0].quantity == 10);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(ob.price_level_array.best_ask() == nullptr);
        Order* r = head_at(ob, 10000);
        CHECK(r != nullptr && r->order_id == 2 && r->quantity == 15 && r->side == 'B');
    }
    std::cout << "Test 3 (PARTIAL - AGGRESSOR BIGGER): PASS\n";

    {   // TEST 4: multi-level sweep
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 5, 'S'});
        ob.add(Order{2, 10001, 8, 'S'});
        auto fills = ob.add(Order{3, 10001, 10, 'B'});
        CHECK(fills.size() == 2);
        CHECK(fills[0].price == 10000);
        CHECK(fills[0].quantity == 5);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(fills[1].price == 10001);
        CHECK(fills[1].quantity == 5);
        CHECK(fills[1].passive_order_id == 2);
        CHECK(head_at(ob, 10000) == nullptr);            // level 10000 emptied
        Order* r = head_at(ob, 10001);
        CHECK(r != nullptr && r->quantity == 3);
        CHECK(ob.price_level_array.best_bid() == nullptr);
    }
    std::cout << "Test 4 (MULTI-LEVEL SWEEP): PASS\n";

    {   // TEST 5: price improvement
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        auto fills = ob.add(Order{2, 10005, 10, 'B'});   // willing to pay 10005
        CHECK(fills.size() == 1);
        CHECK(fills[0].price == 10000);                  // fills at resting price
        CHECK(fills[0].quantity == 10);
        CHECK(ob.price_level_array.best_ask() == nullptr);
        CHECK(ob.price_level_array.best_bid() == nullptr);
    }
    std::cout << "Test 5 (PRICE IMPROVEMENT): PASS\n";

    {   // TEST 6: FIFO survives a partial
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        ob.add(Order{2, 10000, 10, 'S'});
        auto f1 = ob.add(Order{3, 10000, 4, 'B'});
        auto f2 = ob.add(Order{4, 10000, 3, 'B'});
        CHECK(f1.size() == 1 && f1[0].passive_order_id == 1 && f1[0].quantity == 4);
        CHECK(f2.size() == 1 && f2[0].passive_order_id == 1 && f2[0].quantity == 3);
        PriceLevel* lvl = ob.price_level_array.get_price_level(10000);
        CHECK(lvl->head->order_id == 1 && lvl->head->quantity == 3);
        CHECK(lvl->tail->order_id == 2 && lvl->tail->quantity == 10);
    }
    std::cout << "Test 6 (FIFO SURVIVES PARTIAL): PASS\n";

    {   // TEST 7: no cross
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        auto fills = ob.add(Order{2, 9999, 10, 'B'});    // bid below ask, no cross
        CHECK(fills.empty());
        Order* a = head_at(ob, 10000);
        CHECK(a != nullptr && a->quantity == 10);
        Order* b = head_at(ob, 9999);
        CHECK(b != nullptr && b->order_id == 2 && b->quantity == 10);
    }
    std::cout << "Test 7 (NO CROSS): PASS\n";

    {   // TEST 8: sweep exhausts book, remainder rests at own price
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        auto fills = ob.add(Order{2, 10010, 50, 'B'});
        CHECK(fills.size() == 1);
        CHECK(fills[0].price == 10000);
        CHECK(fills[0].quantity == 10);
        CHECK(ob.price_level_array.best_ask() == nullptr);
        Order* b = head_at(ob, 10010);
        CHECK(b != nullptr && b->order_id == 2 && b->quantity == 40);
    }
    std::cout << "Test 8 (SWEEP EXHAUSTS BOOK): PASS\n";

    {   // TEST 9: sell-side multi-level sweep
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10001, 5, 'B'});
        ob.add(Order{2, 10000, 8, 'B'});
        auto fills = ob.add(Order{3, 10000, 10, 'S'});
        CHECK(fills.size() == 2);
        CHECK(fills[0].price == 10001);
        CHECK(fills[0].quantity == 5);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(fills[1].price == 10000);
        CHECK(fills[1].quantity == 5);
        CHECK(fills[1].passive_order_id == 2);
        CHECK(head_at(ob, 10001) == nullptr);
        Order* r = head_at(ob, 10000);
        CHECK(r != nullptr && r->quantity == 3);
        CHECK(ob.price_level_array.best_ask() == nullptr);
    }
    std::cout << "Test 9 (SELL-SIDE SWEEP): PASS\n";

    {   // TEST 10: sell-side partial, aggressor bigger
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'B'});
        auto fills = ob.add(Order{2, 10000, 25, 'S'});
        CHECK(fills.size() == 1);
        CHECK(fills[0].quantity == 10);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(ob.price_level_array.best_bid() == nullptr);
        Order* r = head_at(ob, 10000);
        CHECK(r != nullptr && r->order_id == 2 && r->quantity == 15 && r->side == 'S');
    }
    std::cout << "Test 10 (SELL-SIDE PARTIAL): PASS\n";

    {   // TEST 11: sell-side no cross
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'B'});
        auto fills = ob.add(Order{2, 10001, 10, 'S'});
        CHECK(fills.empty());
        Order* b = head_at(ob, 10000);
        CHECK(b != nullptr && b->quantity == 10);
        Order* a = head_at(ob, 10001);
        CHECK(a != nullptr && a->order_id == 2 && a->quantity == 10);
    }
    std::cout << "Test 11 (SELL-SIDE NO CROSS): PASS\n";

    {   // TEST 12: cancel sole order at a level
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        CHECK(ob.cancel(1) == true);
        CHECK(head_at(ob, 10000) == nullptr);
        CHECK(ob.price_level_array.best_ask() == nullptr);
    }
    std::cout << "Test 12 (CANCEL SOLE): PASS\n";

    {   // TEST 13: cancel one of two at same level
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        ob.add(Order{2, 10000, 10, 'S'});
        CHECK(ob.cancel(1) == true);
        PriceLevel* lvl = ob.price_level_array.get_price_level(10000);
        CHECK(lvl->head != nullptr && lvl->head->order_id == 2);
        CHECK(lvl->tail != nullptr && lvl->tail->order_id == 2);
        CHECK(lvl->head == lvl->tail);
    }
    std::cout << "Test 13 (CANCEL ONE OF TWO): PASS\n";

    {   // TEST 14: cancel nonexistent id
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        CHECK(ob.cancel(99) == false);
        Order* a = head_at(ob, 10000);
        CHECK(a != nullptr && a->order_id == 1 && a->quantity == 10);
    }
    std::cout << "Test 14 (CANCEL NONEXISTENT): PASS\n";

    {   // TEST 15: cancel an already-filled order
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        ob.add(Order{2, 10000, 10, 'B'});     // fully fills id 1
        CHECK(ob.cancel(1) == false);         // id 1 no longer resting
        CHECK(ob.cancel(2) == false);         // id 2 never rested
        CHECK(ob.price_level_array.best_ask() == nullptr);
        CHECK(ob.price_level_array.best_bid() == nullptr);
    }
    std::cout << "Test 15 (CANCEL ALREADY-FILLED): PASS\n";

    {   // TEST 16: cancel a partially-filled remainder
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 25, 'S'});
        ob.add(Order{2, 10000, 10, 'B'});     // leaves 15 resting
        Order* r = head_at(ob, 10000);
        CHECK(r != nullptr && r->quantity == 15);
        CHECK(ob.cancel(1) == true);
        CHECK(head_at(ob, 10000) == nullptr);
        CHECK(ob.price_level_array.best_ask() == nullptr);
    }
    std::cout << "Test 16 (CANCEL PARTIAL REMAINDER): PASS\n";

    {   // TEST 17: replace success
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        auto res = ob.replace(1, Order{2, 10001, 5, 'S'});
        CHECK(res.replaced == true);
        CHECK(res.fills.empty());
        CHECK(head_at(ob, 10000) == nullptr);
        Order* r = head_at(ob, 10001);
        CHECK(r != nullptr && r->order_id == 2 && r->quantity == 5);
    }
    std::cout << "Test 17 (REPLACE SUCCESS): PASS\n";

    {   // TEST 18: replace missing id -> reject, add nothing
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        auto res = ob.replace(99, Order{2, 10000, 5, 'B'});
        CHECK(res.replaced == false);
        CHECK(res.fills.empty());
        CHECK(ob.price_level_array.best_bid() == nullptr);   // new order did NOT rest
        Order* a = head_at(ob, 10000);
        CHECK(a != nullptr && a->order_id == 1 && a->quantity == 10);
    }
    std::cout << "Test 18 (REPLACE MISSING ID): PASS\n";

    {   // TEST 19: replace loses queue position
        OrderBook ob("TEST", 10000);
        ob.add(Order{1, 10000, 10, 'S'});
        ob.add(Order{2, 10000, 10, 'S'});
        auto res = ob.replace(1, Order{3, 10000, 10, 'S'});
        CHECK(res.replaced == true);
        CHECK(res.fills.empty());
        PriceLevel* lvl = ob.price_level_array.get_price_level(10000);
        CHECK(lvl->head->order_id == 2);     // id 2 now first
        CHECK(lvl->tail->order_id == 3);     // replacement at back
    }
    std::cout << "Test 19 (REPLACE LOSES QUEUE POSITION): PASS\n";

    std::cout << "\nALL 19 ORACLE TESTS PASSED\n";
    return 0;
}