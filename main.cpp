#include "OrderbookV2.hpp"
#include <iostream>
#include <cassert>

#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAILED: " #cond "  (line " << __LINE__ << ")\n"; \
    std::abort(); } } while (0)

int main() {
    {   // build chain + unlink MIDDLE
        Order A{1,100,10,'S'}, B{2,100,10,'S'}, C{3,100,10,'S'};
        PriceLevel lvl;
        lvl.push_back(&A); lvl.push_back(&B); lvl.push_back(&C);
        CHECK(lvl.head == &A);
        CHECK(lvl.tail == &C);
        CHECK(A.next == &B && B.prev == &A);
        CHECK(B.next == &C && C.prev == &B);

        lvl.unlink(&B);
        CHECK(A.next == &C && C.prev == &A);
        CHECK(lvl.head == &A && lvl.tail == &C);
    }
    {   // unlink HEAD
        Order A{1,100,10,'S'}, B{2,100,10,'S'}, C{3,100,10,'S'};
        PriceLevel lvl;
        lvl.push_back(&A); lvl.push_back(&B); lvl.push_back(&C);
        lvl.unlink(&A);
        CHECK(lvl.head == &B);
        CHECK(B.prev == nullptr);
        CHECK(lvl.tail == &C);
    }
    {   // unlink TAIL
        Order A{1,100,10,'S'}, B{2,100,10,'S'}, C{3,100,10,'S'};
        PriceLevel lvl;
        lvl.push_back(&A); lvl.push_back(&B); lvl.push_back(&C);
        lvl.unlink(&C);
        CHECK(lvl.tail == &B);
        CHECK(B.next == nullptr);
        CHECK(lvl.head == &A);
    }
    {   // unlink down to EMPTY
        Order A{1,100,10,'S'};
        PriceLevel lvl;
        lvl.push_back(&A);
        lvl.unlink(&A);
        CHECK(lvl.head == nullptr);
        CHECK(lvl.tail == nullptr);
    }

    std::cout << "PriceLevel tests: PASS\n";

   {   // pool: allocate, deallocate, reuse, exhaustion
        OrderPool pool(3);                  // tiny pool so we can drain it
        Order* a = pool.allocate();
        Order* b = pool.allocate();
        Order* c = pool.allocate();
        CHECK(a != nullptr && b != nullptr && c != nullptr);
        CHECK(a != b && b != c);            // distinct slots
        CHECK(pool.allocate() == nullptr);  // exhausted — 4th alloc fails

        pool.deallocate(b);                 // return one
        Order* d = pool.allocate();
        CHECK(d == b);                      // LIFO: freed slot comes back first
    }
    std::cout << "OrderPool tests: PASS\n";


    return 0;
}