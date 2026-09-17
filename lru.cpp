#include <iostream>
#include <unordered_map>
#include <string>
using namespace std;

// Eviction policy. LRU moves a node to the front whenever it is used;
// FIFO never reorders, so order = insertion order and the oldest is evicted.
enum class Policy { LRU, FIFO };

// One slot in the cache. It lives inside the doubly linked list,
// so it knows the node before it (prev) and after it (next).
struct Node {
    int key;
    int value;
    Node* prev;
    Node* next;
    Node(int k, int v) : key(k), value(v), prev(nullptr), next(nullptr) {}
};

class Cache {
private:
    int capacity;                    // max real nodes allowed
    unordered_map<int, Node*> map;   // key -> its node, for O(1) lookup
    Node* head;                      // dummy sentinel: MRU / newest side
    Node* tail;                      // dummy sentinel: LRU / oldest side
    int hits;
    int misses;
    Policy policy;                   // LRU or FIFO
    string name;                     // label for the stats printout

public:
    Cache(int cap, Policy p, string n) {
        capacity = cap;
        policy = p;
        name = n;
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
    ~Cache() {
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
        // Hit. Under LRU, using a key makes it most-recently-used, so move it
        // to the front. Under FIFO, a read does NOT change eviction order.
        Node* node = map[key];
        if (policy == Policy::LRU) {
            remove(node);
            addToFront(node);
        }
        hits++;
        return node->value;
    }

    // Insert or update `key`. On update, refresh the value and mark MRU.
    // On insert into a full cache, evict the LRU node first.
    void put(int key, int value) {
        // Case 1: key already exists -> update value. Under LRU this counts as
        // a use, so move to front; under FIFO the insertion order is unchanged.
        if (map.find(key) != map.end()) {
            Node* node = map[key];
            node->value = value;
            if (policy == Policy::LRU) {
                remove(node);
                addToFront(node);
            }
            return;
        }

        // Case 2: cache is full -> evict the victim just before tail.
        // Same slot for both policies, but for a different reason: under LRU it
        // is the least-recently-used; under FIFO it is the oldest inserted.
        if ((int)map.size() == capacity) {
            Node* victim = tail->prev;  // node at the back of the list
            remove(victim);             // unlink it from the list
            map.erase(victim->key);     // drop it from the map too
            delete victim;              // free the memory
        }

        // Case 3: insert the brand-new node at the front, record it in the map.
        Node* node = new Node(key, value);
        addToFront(node);
        map[key] = node;
    }

    // Hit ratio as a fraction (hits / total accesses); 0 if never accessed.
    double hitRatio() {
        int total = hits + misses;
        return (total == 0) ? 0.0 : (double)hits / total;
    }

    // Report totals and the hit ratio for this cache.
    void printStats() {
        cout << "\n--- " << name << " Stats ---\n";
        cout << "Hits:   " << hits << "\n";
        cout << "Misses: " << misses << "\n";
        cout << "Hit ratio: " << hitRatio() * 100 << "%\n";
    }
};

// Helper for the demo: run a get and print HIT/MISS with the value.
void tryGet(Cache& cache, int key) {
    int v = cache.get(key);
    if (v == -1)
        cout << "  get(" << key << ")  -> MISS\n";
    else
        cout << "  get(" << key << ")  -> HIT (" << v << ")\n";
}

// The SAME sequence of operations, run against whichever cache is passed in.
// Key 1 is "hot": inserted, then accessed repeatedly. Key 4 forces an eviction.
void runWorkload(Cache& cache) {
    cache.put(1, 10);  cout << "  put(1,10)\n";
    cache.put(2, 20);  cout << "  put(2,20)\n";
    cache.put(3, 30);  cout << "  put(3,30)   [cache full]\n";
    tryGet(cache, 1);              // touch the hot key before the eviction
    cache.put(4, 40);  cout << "  put(4,40)   [triggers one eviction]\n";
    tryGet(cache, 1);              // hot key again
    tryGet(cache, 1);              // and again
    tryGet(cache, 3);
}

int main() {
    cout << "Workload (capacity 3): put 1,2,3; get 1; put 4; get 1; get 1; get 3\n";
    cout << "Key 1 is 'hot' - accessed repeatedly after insertion.\n";

    Cache lru(3, Policy::LRU, "LRU");
    cout << "\n=== LRU policy ===\n";
    runWorkload(lru);
    lru.printStats();

    Cache fifo(3, Policy::FIFO, "FIFO");
    cout << "\n=== FIFO policy ===\n";
    runWorkload(fifo);
    fifo.printStats();

    cout << "\n--- Comparison ---\n";
    cout << "LRU  hit ratio: " << lru.hitRatio() * 100 << "%\n";
    cout << "FIFO hit ratio: " << fifo.hitRatio() * 100 << "%\n";
    if (lru.hitRatio() > fifo.hitRatio())
        cout << "LRU wins: it keeps the hot key 1 (recently used), while FIFO\n"
             << "evicts key 1 purely for being oldest - then keeps missing it.\n";
    return 0;
}
