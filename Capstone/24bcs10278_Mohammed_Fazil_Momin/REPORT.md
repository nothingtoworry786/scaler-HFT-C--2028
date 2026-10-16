# Performance Report

Student: Mohammed Fazil Momin  
Roll Number: `24BCS10278`  

Workload: 1,000,000 operations, seed 42, 70% add / 20% cancel / 10% BBO,
`get_bids(10)` and `get_asks(10)` every 100 operations. 5 runs each, median
reported. Built with `g++ -std=c++20 -O2`.

## Results table

Timing rows are from the Windows dev machine. **Replace with the Linux numbers
from `run_perf.sh` and fill the perf rows from `results/perf_stat_*.txt`.**

| Metric | Implementation A | Implementation B | Implementation C |
|---|---|---|---|
| Total time (ms) | 6019 | 1201 | 375 |
| ns / add | 296 | 1195 | 323 |
| ns / cancel | 553 | 912 | 501 |
| ns / BBO | 65 | 507 | 38 |
| IPC | `<perf>` | `<perf>` | `<perf>` |
| Branch misses (%) | `<perf>` | `<perf>` | `<perf>` |
| L1-dcache-load-misses | `<perf>` | `<perf>` | `<perf>` |
| Cache misses | `<perf>` | `<perf>` | `<perf>` |
| Page faults | `<perf>` | `<perf>` | `<perf>` |

Counters unavailable on the machine: `<list, or "none">`

All three end with the same BBO and checksum:

```
Bid: 99.99 x 2542
Ask: 100.01 x 2355
Checksum: 1491614064682522636
```

## perf stat output

### A

```
<paste results/perf_stat_A.txt>
```

### B

```
<paste results/perf_stat_B.txt>
```

### C

```
<paste results/perf_stat_C.txt>
```

## 11.1 Which implementation is fastest?

- **add_order:** A and C are close. B is about 4x slower because a tree insert
  has to compare and rebalance through pointer-linked nodes.
- **cancel_order:** C, then A, then B. C unlinks one list node and subtracts
  from one array slot. B does a tree erase with rebalancing.
- **get_bbo:** C is fastest. It reads two integers and two array slots. B is
  slowest because it walks every order at the best level to sum the quantity.
- **Total time:** A is slowest overall even though its per-op numbers are fine.
  Its depth query sorts all live levels on every call, and that dominates.

## 11.2 Branch prediction

`<fill from perf: which has the highest branch-miss %>`

Expected: B. Every tree insert and erase is a chain of left-or-right decisions
driven by random prices, so the predictor cannot learn them. C's add path is
almost branch free.

## 11.3 Cache behaviour

`<fill from perf: which has the most cache misses>`

Expected: B. Each order is its own heap-allocated tree node, so every operation
follows pointers into scattered memory. C's price levels are one contiguous
array, so finding a level is a single indexed load. A's heap is contiguous but
grows with stale entries, and its hash maps are pointer based.

## 11.4 Page faults

`<fill from perf: which has the most page faults>`

Expected: B, then A, then C. B allocates one node per order. A's heap vector
keeps cancelled orders until they reach the top, so it grows larger than the
live count. C allocates its 10,000 level slots once at start-up.

## 11.5 Production choice

**C.** Lowest latency, but more importantly the most predictable: add, cancel
and BBO each do a fixed small amount of work no matter how many orders are
resting. Memory is bounded and known at start-up. The code is the simplest to
read because the price is the array index.

Trade-offs: C is tied to the spec's price band, so a different instrument
means resizing the arrays. After a cancel empties the best level it scans
toward the mid price, which could be a long walk in a thin book.

B is the one I would not deploy. Its latency grows with the number of live
orders and its BBO cost grows with the size of the best level. A is a middle
ground, but its depth query is too slow and its heap grows without bound.

## 11.6 What I would try next

1. A bitmap over the price levels in C so the best-price scan after a cancel
   becomes one find-first-set instruction.
2. A dense array indexed by order id instead of the hash map, since ids are
   sequential.
3. An object pool for list nodes so per-order allocations come from one arena.
4. Pre-reserving every hash map and vector to the expected order count.
5. Fixing A's depth query by keeping levels sorted instead of sorting on every
   call.
