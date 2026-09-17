# Cache Simulator — LRU vs FIFO (C++)

A small CPU-cache simulator with **O(1)** `get` and `put`, supporting two
eviction policies — **LRU (Least Recently Used)** and **FIFO (First In, First
Out)** — so you can run the same workload through both and compare hit ratios.
It tracks hits, misses, and the final hit ratio.

The LRU design is the same as the classic LeetCode problem
[#146 "LRU Cache"](https://leetcode.com/problems/lru-cache/).

## The idea

A cache holds a limited number of items. When it is full and a new item
arrives, we must evict one. **LRU evicts the item that was used least recently.**

To make both operations O(1), two data structures work together:

- **Doubly linked list** — holds keys in usage order.
  Front = most recently used (MRU), back = least recently used (LRU).
  Using a key moves its node to the front; eviction removes the node at the back.
- **Hash map** (`unordered_map<int, Node*>`) — maps each key to its node in the
  list, so any node is found in O(1) without walking the list.

Together: the hash map gives O(1) **lookup**, the linked list gives O(1)
**reordering and eviction**. Neither alone can do both.

Two dummy **sentinel** nodes (`head`, `tail`) bracket the list so insertion and
removal never need special cases for an empty list or the first/last node.

## LRU vs FIFO

Both policies share the exact same structure and evict the node just before
`tail`. The **only** difference is what happens when a key is *used*:

- **LRU** — a `get` (or a `put` on an existing key) moves that node to the
  front, so order reflects *recency of use*. The victim is the least recently
  used key.
- **FIFO** — using a key does **nothing** to the order, so order reflects
  *insertion time*. The victim is the oldest inserted key, even if it was just
  read.

In code that difference is a single guarded branch:

```cpp
if (policy == Policy::LRU) { remove(node); addToFront(node); }
```

FIFO ties LRU on a no-reuse scan and is cheaper (never reorders), but on
workloads with reused "hot" keys, LRU keeps them and FIFO keeps evicting them —
which is why LRU is the usual default. The demo below shows LRU at 100% vs
FIFO at 50% on the same sequence.

## Operations

| Method              | What it does                                                          |
|---------------------|----------------------------------------------------------------------|
| `Cache(cap, policy, name)` | Construct a cache with `Policy::LRU` or `Policy::FIFO`.        |
| `put(key, value)`   | Insert/update a key. If full, evict the victim first.                |
| `get(key)`          | Return the value (a **hit**) or `-1` (a **miss**).                    |
| `hitRatio()`        | Hits / total accesses, as a fraction.                                |
| `printStats()`      | Print hit count, miss count, and hit ratio.                          |

Under LRU, every `get`/`put` on an existing key marks it most-recently-used.

## Build & run

Dynamic linking is broken on my machine, so build statically:

```bash
g++ -std=c++17 -static lru.cpp -o lru
./lru
```

## Sample output

The same workload (key 1 is "hot" — read repeatedly) run through both policies:

![Demo run comparing LRU and FIFO](demo.png)

```
Workload (capacity 3): put 1,2,3; get 1; put 4; get 1; get 1; get 3

=== LRU policy ===   -> Hits: 4  Misses: 0  Hit ratio: 100%
=== FIFO policy ===  -> Hits: 2  Misses: 2  Hit ratio: 50%
```

Why they diverge: after `put 1,2,3` the order is `[3,2,1]`, so key 1 is oldest.
The `get(1)` moves key 1 to the front **under LRU only**. So `put(4,40)` evicts
key 2 under LRU (keeping the hot key 1) but evicts key 1 under FIFO (oldest) —
after which the repeated `get(1)` calls hit under LRU and miss under FIFO.
A read changes eviction order under LRU; under FIFO it never does.

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| `get`     | O(1) | —     |
| `put`     | O(1) | —     |
| Overall   | —    | O(capacity) |

## Memory

Every `new` has a matching `delete`: evicted nodes are freed in `put`, and the
destructor `~Cache()` frees all survivors (including sentinels) at shutdown,
so nothing leaks.
