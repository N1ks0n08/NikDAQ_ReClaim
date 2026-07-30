// benchmark.cpp — stdlib includes FIRST at global scope so std:: stays global
#include <map>
#include <deque>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <iostream>
#include <random>
#include <chrono>
#include <cstdio>

namespace naive { 
    #include "Orderbook.hpp" 
}
namespace v2    { 
    #include "OrderbookV2.hpp" 
}

using Clock = std::chrono::steady_clock;

// ---- percentile helper --------------------------------------------------
struct Samples {
    std::vector<double> ns;
    void add(double v) { ns.push_back(v); }
    double pct(double p) {
        if (ns.empty()) return 0.0;
        return ns[(size_t)(p * (ns.size() - 1))];
    }
    void sort_() { std::sort(ns.begin(), ns.end()); }
};

// ---- workload -----------------------------------------------------------
struct Op {
    enum Kind { ADD, CANCEL, REPLACE } kind;
    uint64_t order_id, new_id;
    uint32_t price, quantity;
    char side;
};

// Balanced workload that holds the book at ~steady_depth resting orders.
// Preload steady_depth adds, then a measured phase where cancel/replace
// roughly balance adds so book size stays ~constant.
static std::vector<Op> make_balanced(size_t measured_n, uint32_t steady_depth,
                                      uint32_t base_price, uint32_t seed,
                                      size_t& preload_count) {
    std::mt19937_64 rng(seed);
    uint32_t half = std::min<uint32_t>(steady_depth, 700);
    std::uniform_int_distribution<uint32_t> price_dist(base_price - half, base_price + half);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_real_distribution<double> pick(0.0, 1.0);

    std::vector<Op> ops;
    std::vector<uint64_t> live;
    uint64_t next_id = 1;

    // preload to steady_depth (these are timed separately as warm setup)
    for (uint32_t i = 0; i < steady_depth; ++i) {
        Op op{Op::ADD, next_id, 0, price_dist(rng), qty_dist(rng), (rng() & 1) ? 'B' : 'S'};
        live.push_back(next_id++);
        ops.push_back(op);
    }
    preload_count = ops.size();

    // measured phase: balanced add / cancel / replace
    for (size_t i = 0; i < measured_n; ++i) {
        double r = pick(rng);
        Op op{};
        if (r < 0.40 && !live.empty()) {          // 40% cancel
            op.kind = Op::CANCEL;
            size_t idx = rng() % live.size();
            op.order_id = live[idx];
            live[idx] = live.back(); live.pop_back();
        } else if (r < 0.60 && !live.empty()) {   // 20% replace
            op.kind = Op::REPLACE;
            size_t idx = rng() % live.size();
            op.order_id = live[idx];
            op.new_id = next_id++;
            op.price = price_dist(rng); op.quantity = qty_dist(rng);
            op.side = (rng() & 1) ? 'B' : 'S';
            live[idx] = op.new_id;
        } else {                                   // 40% add
            op.kind = Op::ADD;
            op.order_id = next_id++;
            op.price = price_dist(rng); op.quantity = qty_dist(rng);
            op.side = (rng() & 1) ? 'B' : 'S';
            live.push_back(op.order_id);
        }
        ops.push_back(op);
    }
    return ops;
}

// ---- replay with per-op timing -----------------------------------------
// Times each op individually, bucketing samples by kind. Returns sink.
template <typename Book, typename MakeOrder>
static uint64_t replay_timed(Book& ob, const std::vector<Op>& ops, size_t preload,
                             MakeOrder mk,
                             Samples& s_add, Samples& s_cancel, Samples& s_replace,
                             bool timed) {
    uint64_t sink = 0;
    for (size_t i = 0; i < ops.size(); ++i) {
        const auto& op = ops[i];
        bool measure = timed && i >= preload;   // don't time the preload phase
        auto t0 = measure ? Clock::now() : Clock::time_point{};

        if (op.kind == Op::ADD) {
            sink += mk.do_add(ob, op);
        } else if (op.kind == Op::CANCEL) {
            sink += ob.cancel(op.order_id) ? 1 : 0;
        } else {
            sink += mk.do_replace(ob, op);
        }

        if (measure) {
            double ns = std::chrono::duration<double, std::nano>(Clock::now() - t0).count();
            if (op.kind == Op::ADD) s_add.add(ns);
            else if (op.kind == Op::CANCEL) s_cancel.add(ns);
            else s_replace.add(ns);
        }
    }
    return sink;
}

// engine-specific adapters (different Order types, different add/replace returns)
struct NaiveOps {
    uint64_t do_add(naive::OrderBook& ob, const Op& op) {
        auto f = ob.add(naive::Order{op.order_id, op.price, op.quantity, op.side});
        uint64_t s = 0; for (auto& x : f) s += x.quantity; return s;
    }
    uint64_t do_replace(naive::OrderBook& ob, const Op& op) {
        auto r = ob.replace(op.order_id, naive::Order{op.new_id, op.price, op.quantity, op.side});
        uint64_t s = r.replaced ? 1 : 0; for (auto& x : r.fills) s += x.quantity; return s;
    }
};
struct V2Ops {
    uint64_t do_add(v2::OrderBook& ob, const Op& op) {
        auto r = ob.add(v2::Order{op.order_id, op.price, op.quantity, op.side});
        uint64_t s = 0; for (auto& x : r.fills) s += x.quantity; return s;
    }
    uint64_t do_replace(v2::OrderBook& ob, const Op& op) {
        auto r = ob.replace(op.order_id, v2::Order{op.new_id, op.price, op.quantity, op.side});
        uint64_t s = r.canceled ? 1 : 0; for (auto& x : r.add_result.fills) s += x.quantity; return s;
    }
};

static void report(const char* engine, const char* opname, Samples& s) {
    if (s.ns.empty()) { printf("  %-6s %-8s (no samples)\n", engine, opname); return; }
    s.sort_();
    printf("  %-6s %-8s p50=%7.0f  p90=%7.0f  p99=%8.0f  p99.9=%9.0f  max=%10.0f\n",
           engine, opname, s.pct(0.50), s.pct(0.90), s.pct(0.99), s.pct(0.999), s.ns.back());
}

int main() {
    const uint32_t base = 10000;
    const size_t   measured_n = 1000000;
    uint32_t depths[] = {100, 1000, 10000, 100000};

    volatile uint64_t global_sink = 0;

    // ---- throughput sweep ----
    printf("=== THROUGHPUT (balanced mix, ns/op) ===\n");
    printf("%-10s %14s %14s %8s\n", "depth", "naive ns/op", "v2 ns/op", "speedup");
    for (uint32_t d : depths) {
        size_t preload;
        auto ops = make_balanced(measured_n, d, base, 999, preload);

        // naive
        { naive::OrderBook ob; NaiveOps mk; Samples a,c,r;
          replay_timed(ob, ops, preload, mk, a,c,r, false);       // warm
        }
        double naive_ns, v2_ns;
        { naive::OrderBook ob; NaiveOps mk; Samples a,c,r;
          replay_timed(ob, ops, preload, mk, a,c,r, false);       // warm again (fresh book)
          auto t0 = Clock::now();
          global_sink += replay_timed(ob, ops, preload, mk, a,c,r, false);
          naive_ns = std::chrono::duration<double,std::nano>(Clock::now()-t0).count() / ops.size();
        }
        { v2::OrderBook ob("BENCH", base); V2Ops mk; Samples a,c,r;
          replay_timed(ob, ops, preload, mk, a,c,r, false);
          auto t0 = Clock::now();
          global_sink += replay_timed(ob, ops, preload, mk, a,c,r, false);
          v2_ns = std::chrono::duration<double,std::nano>(Clock::now()-t0).count() / ops.size();
        }
        printf("%-10u %14.2f %14.2f %7.2fx\n", d, naive_ns, v2_ns, naive_ns/v2_ns);
    }

    // ---- percentile detail at two representative depths ----
    uint32_t pct_depths[] = {1000, 100000};
    for (uint32_t d : pct_depths) {
        printf("\n=== TAIL LATENCY at depth %u (ns) ===\n", d);
        size_t preload;
        auto ops = make_balanced(measured_n, d, base, 999, preload);

        { naive::OrderBook ob; NaiveOps mk; Samples a,c,r;
          replay_timed(ob, ops, preload, mk, a,c,r, false);       // warm
          naive::OrderBook ob2; Samples a2,c2,r2;
          global_sink += replay_timed(ob2, ops, preload, mk, a2,c2,r2, true);
          report("naive","add",a2); report("naive","cancel",c2); report("naive","replace",r2);
        }
        { v2::OrderBook ob("BENCH", base); V2Ops mk; Samples a,c,r;
          replay_timed(ob, ops, preload, mk, a,c,r, false);
          v2::OrderBook ob2("BENCH", base); Samples a2,c2,r2;
          global_sink += replay_timed(ob2, ops, preload, mk, a2,c2,r2, true);
          report("v2","add",a2); report("v2","cancel",c2); report("v2","replace",r2);
        }
    }

    printf("\n(sink=%llu)\n", (unsigned long long)global_sink);
    return 0;
}