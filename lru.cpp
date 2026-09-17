#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
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

// One parsed operation. type 'p' = put (uses key+value), 'g' = get (uses key).
// We parse the whole workload into a list of these so it can be REPLAYED
// through more than one cache (a file stream can only be read once).
struct Op {
    char type;
    int key;
    int value;
};

// Run a get and print HIT/MISS with the value.
void tryGet(Cache& cache, int key) {
    int v = cache.get(key);
    if (v == -1)
        cout << "  get(" << key << ")  -> MISS\n";
    else
        cout << "  get(" << key << ")  -> HIT (" << v << ")\n";
}

// Execute a parsed workload against one cache, printing each step.
void runOps(Cache& cache, const vector<Op>& ops) {
    for (const Op& op : ops) {
        if (op.type == 'p') {
            cache.put(op.key, op.value);
            cout << "  put(" << op.key << "," << op.value << ")\n";
        } else {
            tryGet(cache, op.key);
        }
    }
}

// Built-in workload used when no file is given. Key 1 is "hot" (read repeatedly).
vector<Op> defaultOps() {
    return { {'p',1,10}, {'p',2,20}, {'p',3,30}, {'g',1,0},
             {'p',4,40}, {'g',1,0}, {'g',1,0}, {'g',3,0} };
}

// Parse a workload file into `ops`, updating `capacity` if a `cap` line is seen.
// Format (one directive per line): `cap N`, `put KEY VALUE`, `get KEY`.
// Blank lines and lines starting with '#' are ignored. Returns false if the
// file can't be opened.
bool loadOps(const string& path, vector<Op>& ops, int& capacity) {
    ifstream in(path);
    if (!in) return false;

    string line;
    while (getline(in, line)) {
        istringstream iss(line);
        string cmd;
        if (!(iss >> cmd)) continue;        // blank / whitespace-only line
        if (cmd[0] == '#') continue;        // comment

        if (cmd == "cap") {
            iss >> capacity;
        } else if (cmd == "put") {
            int k, v;
            iss >> k >> v;
            ops.push_back({'p', k, v});
        } else if (cmd == "get") {
            int k;
            iss >> k;
            ops.push_back({'g', k, 0});
        } else {
            cerr << "  (skipping unknown command: " << cmd << ")\n";
        }
    }
    return true;
}

int main(int argc, char* argv[]) {
    int capacity = 3;
    vector<Op> ops;
    string source;

    if (argc > 1) {
        // File-driven: read the workload from the given path.
        if (!loadOps(argv[1], ops, capacity)) {
            cerr << "Error: could not open workload file '" << argv[1] << "'\n";
            return 1;
        }
        source = argv[1];
    } else {
        // No file given: fall back to the built-in demo workload.
        ops = defaultOps();
        source = "built-in demo (pass a file path to use your own)";
    }

    cout << "Workload source: " << source << "\n";
    cout << "Capacity: " << capacity << ", operations: " << ops.size() << "\n";

    Cache lru(capacity, Policy::LRU, "LRU");
    cout << "\n=== LRU policy ===\n";
    runOps(lru, ops);
    lru.printStats();

    Cache fifo(capacity, Policy::FIFO, "FIFO");
    cout << "\n=== FIFO policy ===\n";
    runOps(fifo, ops);
    fifo.printStats();

    cout << "\n--- Comparison ---\n";
    cout << "LRU  hit ratio: " << lru.hitRatio() * 100 << "%\n";
    cout << "FIFO hit ratio: " << fifo.hitRatio() * 100 << "%\n";
    if (lru.hitRatio() > fifo.hitRatio())
        cout << "LRU wins on this workload: it keeps recently-used keys, while\n"
             << "FIFO evicts purely by insertion age.\n";
    else if (fifo.hitRatio() > lru.hitRatio())
        cout << "FIFO wins on this workload.\n";
    else
        cout << "Both policies tie on this workload.\n";
    return 0;
}
