# LRU Cache Simulator (C++)

A small CPU-cache simulator that implements the **LRU (Least Recently Used)**
eviction policy in **O(1)** time for both `get` and `put`. It tracks hits,
misses, and the final hit ratio.

This is the same design as the classic LeetCode problem
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

## Operations

| Method              | What it does                                                         |
|---------------------|---------------------------------------------------------------------|
| `put(key, value)`   | Insert/update a key. If full, evict the LRU node first.             |
| `get(key)`          | Return the value (a **hit**) or `-1` (a **miss**); marks key as MRU. |
| `printStats()`      | Print hit count, miss count, and hit ratio.                         |

Every `get`/`put` on an existing key marks it most-recently-used.

## Build & run

Dynamic linking is broken on my machine, so build statically:

```bash
g++ -std=c++17 -static lru.cpp -o lru
./lru
```

## Sample output

![Demo run of the LRU cache simulator](demo.png)

```
Cache created with capacity 3.

put(1,10)
put(2,20)
put(3,30)   [cache full: 3,2,1]
get(1)  -> HIT (10)
put(4,40)   [evicts key 2, the LRU]
get(2)  -> MISS
get(3)  -> HIT (30)
get(4)  -> HIT (40)

--- Cache Stats ---
Hits:   3
Misses: 1
Hit ratio: 75%
```

Note the key moment: `get(1)` moves key 1 to the front, so key **2** becomes the
least-recently-used and is the one evicted by `put(4,40)`. Without that access,
key 1 would have been evicted instead — a read changes eviction order.

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| `get`     | O(1) | —     |
| `put`     | O(1) | —     |
| Overall   | —    | O(capacity) |

## Memory

Every `new` has a matching `delete`: evicted nodes are freed in `put`, and the
destructor `~LRUCache()` frees all survivors (including sentinels) at shutdown,
so nothing leaks.
