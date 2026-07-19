#include "Orderbook.hpp"
#include <iostream>
#include <cassert>

#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAILED: " #cond "  (line " << __LINE__ << ")\n"; \
    std::abort(); } } while (0)

int main() {
    {
        // TEST: Single full fill at resting price
        // set resting SELL order of 25 shares @ $100
        OrderBook orderbook;

        Order resting = {
            1,
            100,
            25,
            'S'
        };
        auto rest_fills = orderbook.add(resting);
        CHECK(rest_fills.empty()); // Initial resting order, no fills since it's the only order as of now

        // set aggressing BUY order of 25 shares @ $100
        Order aggressor = {
            2,
            100,
            25,
            'B'
        };
        auto fills = orderbook.add(aggressor);

        CHECK(fills.size() == 1); // only 1 resting order, so only 1 fill
        CHECK(fills[0].price == 100);
        CHECK(fills[0].aggressive_order_id == 2);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(fills[0].quantity == 25);
    }
    std::cout << "Test case 1 (SINGLE FULL FILL) result: PASS\n";

    
    {
        // TEST: PARTIAL FILL (RESTING BIGGER)
        /*
            Scenario: 
            Rest order: SELL 25 @ 100
            Aggressor order: BUY 10 @ 100
        */
        OrderBook orderbook;
        Order resting = {
            1,
            100,
            25,
            'S'
        };

        auto rest_fills = orderbook.add(resting);

        Order aggressor = {
            2,
            100,
            10,
            'B'
        };

        auto aggressor_fills = orderbook.add(aggressor);
        // aggressor has filled 10 of 25 resting 
        // resting should have 15 shares remaining
        // there should be nothing in bids since aggressor got filled
        CHECK(rest_fills.empty());
        CHECK(aggressor_fills.size() == 1);
        CHECK(aggressor_fills[0].price == 100);
        CHECK(aggressor_fills[0].quantity == 10);
        CHECK(aggressor_fills[0].aggressive_order_id == 2);
        CHECK(aggressor_fills[0].passive_order_id == 1);
        CHECK(orderbook.asks.size() == 1);
        CHECK(orderbook.asks.at(100).size() == 1);
        CHECK(orderbook.asks.at(100).front().quantity == 15);
        CHECK(orderbook.bids.empty());
    }
    std:: cout << "Test case 2 (PARTIAL FILL - RESTING BIGGER) result: PASS\n";

    {
        // TEST: PARTIAL FILL (AGGRESSOR BIGGER)
        /*
            Scenario: 
            Rest order: SELL 10 @@ 100
            Aggressor order: BUY 25 @ 100
        */
       OrderBook orderbook;
       Order resting = {
            1,
            100,
            10,
            'S'
       };

       auto rest_fills = orderbook.add(resting);

       Order aggressor = {
            2,
            100,
            25,
            'B'
       };

       auto aggressor_fills = orderbook.add(aggressor);
       // aggressor has filled 10 of 10 resting shares
       // aggressor has 15 shares remaining
       // there should be nothing in asks since the resting side is completely filled
        CHECK(aggressor_fills.size() == 1);
        CHECK(aggressor_fills[0].price == 100);
        CHECK(aggressor_fills[0].quantity == 10);
        CHECK(aggressor_fills[0].passive_order_id == 1);
        CHECK(orderbook.asks.empty());
        CHECK(orderbook.asks.count(100) == 0);
        CHECK(orderbook.bids.size() == 1);
        CHECK(orderbook.bids.at(100).front().order_id == 2);
        CHECK(orderbook.bids.at(100).front().quantity == 15);
    }

    std::cout << "Test case 3 (PARTIAL FILL - AGGRESSOR BIGGER) result: PASS\n";

    {
        // TEST: MULTI-LEVEL SWEEP
        /*
            Scenario:
            Resting orders: 
            SELL 5 @ 100 (id 1)
            SELL 8 @ 101 (id 2)

            Agressor order:
            BUY 10 @ 101 (id 3)

            Expected result:
            First fill: 5 shares @ 100
            Second fill: 5 shares @ 101
            Book state: SELL 3 @ 101 remaining, bids are completely filled
        */

        OrderBook orderbook;
        Order resting_1 = {
            1,
            100,
            5,
            'S'
        };
        Order resting_2 = {
            2,
            101,
            8,
            'S'
        };
        auto resting_fill1 = orderbook.add(resting_1);
        auto resting_fill2 = orderbook.add(resting_2);

        Order aggressor = {
            3,
            101,
            10,
            'B'
        };

        auto aggressor_fills = orderbook.add(aggressor);
        CHECK(aggressor_fills.size() == 2);
        CHECK(aggressor_fills[0].price == 100);
        CHECK(aggressor_fills[0].quantity == 5);
        CHECK(aggressor_fills[0].passive_order_id == 1);
        CHECK(aggressor_fills[1].price == 101);
        CHECK(aggressor_fills[1].quantity == 5);
        CHECK(aggressor_fills[1].passive_order_id == 2);
        CHECK(orderbook.asks.count(100) == 0);
        CHECK(orderbook.asks.size() == 1);
        CHECK(orderbook.asks.at(101).size() == 1);
        CHECK(orderbook.asks.at(101).front().quantity == 3);
        CHECK(orderbook.bids.empty());
    }
    std::cout << "Test case 4 (MULTI-LEVEL SWEEP) result: PASS\n";

    {
        // TEST: PRICE IMPROVEMENT
        /*
            Scenario:
            Resting orders: SELL 10 @ 100 (id 1)
            Aggressor orders: BUY 10 @ 105 (id 2)

            Expected results:
            Order fill @ 100, not 105
            Both sides empty
        */

        OrderBook orderbook;
        Order resting = {
            1,
            100,
            10,
            'S'
        };
        auto resting_fills = orderbook.add(resting);

        Order aggressor = {
            2,
            105,
            10,
            'B'
        };
        auto aggressor_fills = orderbook.add(aggressor);
        
        CHECK(aggressor_fills.size() == 1);
        CHECK(aggressor_fills[0].price == 100);
        CHECK(aggressor_fills[0].quantity == 10);
        CHECK(orderbook.asks.empty());
        CHECK(orderbook.bids.empty());
    }
    std::cout << "Ttest case 5 (PRICE IMPROVEMENT) result: PASS\n";

    {
        // TEST: FIFO SURVIVES A PAETIAL
        /*
            Scenario:
            Resting ordres: 
            SELL 10 @ 100 (id 1)
            SELL 10 @ 100 (id 2)

            Aggressor orders:
            BUY 4 @ 100 (id 3)
            BUY 3 @ 100 (id 4)

            Expected result:
            Both aggressors 1 and 2 are filled against resting order with id 1 since it was added first.
            resting order id 1 still has 3 shares (since (10 - 4) - 3 = 3) 
            and thus, in the deque of price level 100, front() on the deque should still return order id of 1
        */

        OrderBook orderbook;

        Order resting_1 = {
            1,
            100,
            10,
            'S'
        };
        Order resting_2 = {
            2,
            100,
            10,
            'S'
        };

        auto resting_fills_1 = orderbook.add(resting_1);
        auto resting_fills_2 = orderbook.add(resting_2);

        Order aggressor_1 = {
            3,
            100,
            4,
            'B'
        };

        Order aggressor_2 = {
            4,
            100,
            3,
            'B'
        };

        auto aggressor_fill_1 = orderbook.add(aggressor_1);
        auto aggressor_fill_2 = orderbook.add(aggressor_2);

        CHECK(aggressor_fill_1.size() == 1);
        CHECK(aggressor_fill_1[0].passive_order_id == 1);
        CHECK(aggressor_fill_1[0].quantity == 4);
        CHECK(aggressor_fill_2.size() == 1);
        CHECK(aggressor_fill_2[0].passive_order_id == 1);
        CHECK(aggressor_fill_2[0].quantity == 3);
        CHECK(orderbook.asks.at(100).size() == 2);
        CHECK(orderbook.asks.at(100).front().order_id == 1);
        CHECK(orderbook.asks.at(100).front().quantity == 3);
        CHECK(orderbook.asks.at(100).back().order_id == 2);
        CHECK(orderbook.asks.at(100).back().quantity == 10);
    }
    std::cout << "Test case 6 (FIFO SURVIVES A PARTIAL) result: PASS\n"; 

    {
        // TEST: NO CROSS
        /*
            Resting: SELL 10 @ 100 (id 1)
            Aggressor: BUY 10 @ 99 (id 2)
            Expected: no fills; both orders rest on their own sides
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        Order aggressor = { 2, 99, 10, 'B' };
        auto fills = orderbook.add(aggressor);

        CHECK(fills.empty());
        CHECK(orderbook.asks.size() == 1);
        CHECK(orderbook.asks.at(100).front().quantity == 10);
        CHECK(orderbook.bids.size() == 1);
        CHECK(orderbook.bids.at(99).front().order_id == 2);
        CHECK(orderbook.bids.at(99).front().quantity == 10);
    }
    std::cout << "Test case 7 (NO CROSS) result: PASS\n";

    {
        // TEST: SWEEP EXHAUSTS BOOK
        /*
            Resting: SELL 10 @ 100 (id 1)
            Aggressor: BUY 50 @ 110 (id 2)
            Expected: one fill of 10 @ 100; asks empty; 40 rests in bids AT 110 (its own price)
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        Order aggressor = { 2, 110, 50, 'B' };
        auto fills = orderbook.add(aggressor);

        CHECK(fills.size() == 1);
        CHECK(fills[0].price == 100);
        CHECK(fills[0].quantity == 10);
        CHECK(orderbook.asks.empty());
        CHECK(orderbook.bids.size() == 1);
        CHECK(orderbook.bids.at(110).front().order_id == 2);
        CHECK(orderbook.bids.at(110).front().quantity == 40);
    }
    std::cout << "Test case 8 (SWEEP EXHAUSTS BOOK) result: PASS\n";

    {
        // TEST: SELL-SIDE MULTI-LEVEL SWEEP (mirror of test 4)
        /*
            Resting: BUY 5 @ 101 (id 1), BUY 8 @ 100 (id 2)
            Aggressor: SELL 10 @ 100 (id 3)
            Expected: fill 5 @ 101 vs id 1, then fill 5 @ 100 vs id 2;
                      level 101 erased; 3 left on id 2 @ 100
        */
        OrderBook orderbook;
        Order resting_1 = { 1, 101, 5, 'B' };
        Order resting_2 = { 2, 100, 8, 'B' };
        orderbook.add(resting_1);
        orderbook.add(resting_2);

        Order aggressor = { 3, 100, 10, 'S' };
        auto fills = orderbook.add(aggressor);

        CHECK(fills.size() == 2);
        CHECK(fills[0].price == 101);
        CHECK(fills[0].quantity == 5);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(fills[1].price == 100);
        CHECK(fills[1].quantity == 5);
        CHECK(fills[1].passive_order_id == 2);
        CHECK(orderbook.bids.count(101) == 0);
        CHECK(orderbook.bids.size() == 1);
        CHECK(orderbook.bids.at(100).front().quantity == 3);
        CHECK(orderbook.asks.empty());
    }
    std::cout << "Test case 9 (SELL-SIDE MULTI-LEVEL SWEEP) result: PASS\n";

    {
        // TEST: SELL-SIDE PARTIAL (AGGRESSOR BIGGER)
        /*
            Resting: BUY 10 @ 100 (id 1)
            Aggressor: SELL 25 @ 100 (id 2)
            Expected: one fill of 10 @ 100; bids empty; 15 rests in asks @ 100
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'B' };
        orderbook.add(resting);

        Order aggressor = { 2, 100, 25, 'S' };
        auto fills = orderbook.add(aggressor);

        CHECK(fills.size() == 1);
        CHECK(fills[0].price == 100);
        CHECK(fills[0].quantity == 10);
        CHECK(fills[0].passive_order_id == 1);
        CHECK(orderbook.bids.empty());
        CHECK(orderbook.asks.at(100).front().order_id == 2);
        CHECK(orderbook.asks.at(100).front().quantity == 15);
    }
    std::cout << "Test case 10 (SELL-SIDE PARTIAL - AGGRESSOR BIGGER) result: PASS\n";

    {
        // TEST: SELL-SIDE NO CROSS
        /*
            Resting: BUY 10 @ 100 (id 1)
            Aggressor: SELL 10 @ 101 (id 2)
            Expected: no fills; both rest
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'B' };
        orderbook.add(resting);

        Order aggressor = { 2, 101, 10, 'S' };
        auto fills = orderbook.add(aggressor);

        CHECK(fills.empty());
        CHECK(orderbook.bids.at(100).front().quantity == 10);
        CHECK(orderbook.asks.at(101).front().order_id == 2);
        CHECK(orderbook.asks.at(101).front().quantity == 10);
    }
    std::cout << "Test case 11 (SELL-SIDE NO CROSS) result: PASS\n";

    {
        // TEST: CANCEL SOLE ORDER AT A LEVEL
        /*
            Resting: SELL 10 @ 100 (id 1)
            cancel(1) -> true; level erased, book empty
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        CHECK(orderbook.cancel(1) == true);
        CHECK(orderbook.asks.count(100) == 0);
        CHECK(orderbook.asks.empty());
    }
    std::cout << "Test case 12 (CANCEL SOLE ORDER) result: PASS\n";

    {
        // TEST: CANCEL ONE OF TWO AT SAME LEVEL
        /*
            Resting: SELL 10 @ 100 (id 1), SELL 10 @ 100 (id 2)
            cancel(1) -> true; level survives with id 2 alone at the front
        */
        OrderBook orderbook;
        Order resting_1 = { 1, 100, 10, 'S' };
        Order resting_2 = { 2, 100, 10, 'S' };
        orderbook.add(resting_1);
        orderbook.add(resting_2);

        CHECK(orderbook.cancel(1) == true);
        CHECK(orderbook.asks.size() == 1);
        CHECK(orderbook.asks.at(100).size() == 1);
        CHECK(orderbook.asks.at(100).front().order_id == 2);
        CHECK(orderbook.asks.at(100).front().quantity == 10);
    }
    std::cout << "Test case 13 (CANCEL ONE OF TWO) result: PASS\n";

    {
        // TEST: CANCEL NONEXISTENT ID
        /*
            Resting: SELL 10 @ 100 (id 1)
            cancel(99) -> false; book completely unchanged
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        CHECK(orderbook.cancel(99) == false);
        CHECK(orderbook.asks.size() == 1);
        CHECK(orderbook.asks.at(100).size() == 1);
        CHECK(orderbook.asks.at(100).front().order_id == 1);
        CHECK(orderbook.asks.at(100).front().quantity == 10);
    }
    std::cout << "Test case 14 (CANCEL NONEXISTENT) result: PASS\n";

    {
        // TEST: CANCEL AN ALREADY-FILLED ORDER
        /*
            Resting: SELL 10 @ 100 (id 1), fully filled by BUY 10 @ 100 (id 2)
            cancel(1) -> false (no longer resting)
            cancel(2) -> false (aggressor never rested)
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        Order aggressor = { 2, 100, 10, 'B' };
        auto fills = orderbook.add(aggressor);
        CHECK(fills.size() == 1);

        CHECK(orderbook.cancel(1) == false);
        CHECK(orderbook.cancel(2) == false);
        CHECK(orderbook.asks.empty());
        CHECK(orderbook.bids.empty());
    }
    std::cout << "Test case 15 (CANCEL ALREADY-FILLED) result: PASS\n";

    {
        // TEST: CANCEL A PARTIALLY-FILLED REMAINDER
        /*
            Resting: SELL 25 @ 100 (id 1); BUY 10 @ 100 (id 2) takes 10, leaving 15
            cancel(1) -> true; the 15-share remainder is removed, level erased
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 25, 'S' };
        orderbook.add(resting);

        Order aggressor = { 2, 100, 10, 'B' };
        orderbook.add(aggressor);
        CHECK(orderbook.asks.at(100).front().quantity == 15);

        CHECK(orderbook.cancel(1) == true);
        CHECK(orderbook.asks.count(100) == 0);
        CHECK(orderbook.asks.empty());
    }
    std::cout << "Test case 16 (CANCEL PARTIAL REMAINDER) result: PASS\n";

    {
        // TEST: REPLACE SUCCESS
        /*
            Resting: SELL 10 @ 100 (id 1)
            replace(1, SELL 5 @ 101 (id 2))
            Expected: replaced == true, no fills (nothing to cross),
                      old level gone, new order resting @ 101
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        Order new_order = { 2, 101, 5, 'S' };
        auto result = orderbook.replace(1, new_order);

        CHECK(result.replaced == true);
        CHECK(result.fills.empty());
        CHECK(orderbook.asks.count(100) == 0);
        CHECK(orderbook.asks.size() == 1);
        CHECK(orderbook.asks.at(101).front().order_id == 2);
        CHECK(orderbook.asks.at(101).front().quantity == 5);
    }
    std::cout << "Test case 17 (REPLACE SUCCESS) result: PASS\n";

    {
        // TEST: REPLACE WITH MISSING ID (reject semantics)
        /*
            Resting: SELL 10 @ 100 (id 1)
            replace(99, BUY 5 @ 100 (id 2))
            Expected: replaced == false, no fills, and CRITICALLY the new
                      order must NOT have been added to the book
        */
        OrderBook orderbook;
        Order resting = { 1, 100, 10, 'S' };
        orderbook.add(resting);

        Order new_order = { 2, 100, 5, 'B' };
        auto result = orderbook.replace(99, new_order);

        CHECK(result.replaced == false);
        CHECK(result.fills.empty());
        CHECK(orderbook.bids.empty());          // new order did NOT rest
        CHECK(orderbook.asks.size() == 1);      // original untouched
        CHECK(orderbook.asks.at(100).front().order_id == 1);
        CHECK(orderbook.asks.at(100).front().quantity == 10);
    }
    std::cout << "Test case 18 (REPLACE MISSING ID) result: PASS\n";

    {
        // TEST: REPLACE LOSES QUEUE POSITION
        /*
            Resting: SELL 10 @ 100 (id 1) then SELL 10 @ 100 (id 2)
            replace(1, SELL 10 @ 100 (id 3))
            Expected: id 1 removed; id 3 goes to the BACK, behind id 2,
                      even though id 1 was originally first
        */
        OrderBook orderbook;
        Order resting_1 = { 1, 100, 10, 'S' };
        Order resting_2 = { 2, 100, 10, 'S' };
        orderbook.add(resting_1);
        orderbook.add(resting_2);

        Order new_order = { 3, 100, 10, 'S' };
        auto result = orderbook.replace(1, new_order);

        CHECK(result.replaced == true);
        CHECK(result.fills.empty());
        CHECK(orderbook.asks.at(100).size() == 2);
        CHECK(orderbook.asks.at(100).front().order_id == 2);   // id 2 now first
        CHECK(orderbook.asks.at(100).back().order_id == 3);    // replacement at back
    }
    std::cout << "Test case 19 (REPLACE LOSES QUEUE POSITION) result: PASS\n";

    return 0;
}
