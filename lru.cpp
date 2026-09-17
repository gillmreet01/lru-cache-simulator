#include <iostream>
#include <unordered_map>
using namespace std;

// One slot in the cache. It lives inside the doubly linked list,
// so it knows the node before it (prev) and after it (next).
struct Node {
    int key;
    int value;
    Node* prev;
    Node* next;
    Node(int k, int v) : key(k), value(v), prev(nullptr), next(nullptr) {}
};

class LRUCache {
private:
    int capacity;                    // max real nodes allowed
    unordered_map<int, Node*> map;   // key -> its node, for O(1) lookup
    Node* head;                      // dummy sentinel: MRU side
    Node* tail;                      // dummy sentinel: LRU side
    int hits;
    int misses;

public:
    LRUCache(int cap) {
        capacity = cap;
        hits = 0;
        misses = 0;

        // Create the two dummy nodes and link them to each other.
        // Real nodes will always be inserted BETWEEN head and tail.
        head = new Node(0, 0);
        tail = new Node(0, 0);
        head->next = tail;
        tail->prev = head;
    }

    // Free every node in the list, sentinels included, so nothing leaks.
    ~LRUCache() {
        Node* cur = head;
        while (cur != nullptr) {
            Node* next = cur->next;  // save the next pointer BEFORE deleting cur
            delete cur;
            cur = next;
        }
    }

private:
    // Splice `node` in right after head (the MRU position).
    // Before:  head <-> X
    // After:   head <-> node <-> X
    void addToFront(Node* node) {
        node->prev = head;          // node's left neighbour is head
        node->next = head->next;    // node's right neighbour is the old first node
        head->next->prev = node;    // old first node now points back to node
        head->next = node;          // head now points forward to node
    }

    // Unlink `node` from the list. Its neighbours join hands over the gap.
    // Before:  A <-> node <-> B
    // After:   A <-> B
    void remove(Node* node) {
        node->prev->next = node->next;  // left neighbour skips node
        node->next->prev = node->prev;  // right neighbour skips node
    }

public:
    // Return the value for `key` if present (a HIT), else -1 (a MISS).
    // A hit also marks the key as most-recently-used.
    int get(int key) {
        if (map.find(key) == map.end()) {
            misses++;
            return -1;              // not in cache
        }
        // Hit: pull the node out and re-insert it at the front (MRU).
        Node* node = map[key];
        remove(node);
        addToFront(node);
        hits++;
        return node->value;
    }

    // Insert or update `key`. On update, refresh the value and mark MRU.
    // On insert into a full cache, evict the LRU node first.
    void put(int key, int value) {
        // Case 1: key already exists -> update value, move to front.
        if (map.find(key) != map.end()) {
            Node* node = map[key];
            node->value = value;
            remove(node);
            addToFront(node);
            return;
        }

        // Case 2: cache is full -> evict the LRU node (just before tail).
        if ((int)map.size() == capacity) {
            Node* lru = tail->prev;     // the least-recently-used real node
            remove(lru);                // unlink it from the list
            map.erase(lru->key);        // drop it from the map too
            delete lru;                 // free the memory
        }

        // Case 3: insert the brand-new node at the front, record it in the map.
        Node* node = new Node(key, value);
        addToFront(node);
        map[key] = node;
    }

    // Report totals and the hit ratio (hits / total accesses).
    void printStats() {
        int total = hits + misses;
        double ratio = (total == 0) ? 0.0 : (double)hits / total;
        cout << "\n--- Cache Stats ---\n";
        cout << "Hits:   " << hits << "\n";
        cout << "Misses: " << misses << "\n";
        cout << "Hit ratio: " << ratio * 100 << "%\n";
    }
};

// Helper for the demo: run a get and print HIT/MISS with the value.
void tryGet(LRUCache& cache, int key) {
    int v = cache.get(key);
    if (v == -1)
        cout << "get(" << key << ")  -> MISS\n";
    else
        cout << "get(" << key << ")  -> HIT (" << v << ")\n";
}

int main() {
    LRUCache cache(3);
    cout << "Cache created with capacity 3.\n\n";

    cache.put(1, 10);   cout << "put(1,10)\n";
    cache.put(2, 20);   cout << "put(2,20)\n";
    cache.put(3, 30);   cout << "put(3,30)   [cache full: 3,2,1]\n";

    tryGet(cache, 1);   // HIT -> makes 1 most-recent, so 2 is now LRU

    cache.put(4, 40);   cout << "put(4,40)   [evicts key 2, the LRU]\n";

    tryGet(cache, 2);   // MISS -> 2 was evicted
    tryGet(cache, 3);   // HIT (30)
    tryGet(cache, 4);   // HIT (40)

    cache.printStats();
    return 0;
}
