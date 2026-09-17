#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <optional>
using namespace std;

// Eviction policy. LRU moves a node to the front whenever it is used;
// FIFO never reorders, so order = insertion order and the oldest is evicted.
enum class Policy { LRU, FIFO };

// One slot in the cache, generic over key type K and value type V.
// It lives inside the doubly linked list, so it knows its neighbours.
template <typename K, typename V>
struct Node {
    K key;
    V value;
    Node* prev;
    Node* next;
    Node(K k, V v) : key(k), value(v), prev(nullptr), next(nullptr) {}
};

// A cache generic over key type K and value type V.
// K must be usable as an unordered_map key (i.e. have a std::hash), and both
// K and V must be default-constructible (the sentinels hold default values).
template <typename K, typename V>
class Cache {
private:
    using NodeT = Node<K, V>;        // shorthand for this cache's node type

    int capacity;                    // max real nodes allowed
    unordered_map<K, NodeT*> map;    // key -> its node, for O(1) lookup
    NodeT* head;                     // dummy sentinel: MRU / newest side
    NodeT* tail;                     // dummy sentinel: LRU / oldest side
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

        // Sentinels hold default-constructed junk; real nodes go between them.
        head = new NodeT(K(), V());
        tail = new NodeT(K(), V());
        head->next = tail;
        tail->prev = head;
    }

    // Free every node in the list, sentinels included, so nothing leaks.
    ~Cache() {
        NodeT* cur = head;
        while (cur != nullptr) {
            NodeT* next = cur->next;  // save next BEFORE deleting cur
            delete cur;
            cur = next;
        }
    }

private:
    // Splice `node` in right after head (the MRU position).
    void addToFront(NodeT* node) {
        node->prev = head;
        node->next = head->next;
        head->next->prev = node;
        head->next = node;
    }

    // Unlink `node`; its neighbours join hands over the gap.
    void remove(NodeT* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    }

public:
    // Return the value for `key` if present (a HIT), else an empty optional
    // (a MISS). We can no longer use a sentinel like -1, because V is any type;
    // optional<V> cleanly means "a value, or nothing" for every type.
    // Under LRU a hit also marks the key most-recently-used.
    optional<V> get(const K& key) {
        auto it = map.find(key);
        if (it == map.end()) {
            misses++;
            return nullopt;
        }
        NodeT* node = it->second;
        if (policy == Policy::LRU) {
            remove(node);
            addToFront(node);
        }
        hits++;
        return node->value;
    }

    // Insert or update `key`. If full, evict the victim (just before tail).
    void put(const K& key, const V& value) {
        // Case 1: key already exists -> update value; under LRU, move to front.
        auto it = map.find(key);
        if (it != map.end()) {
            NodeT* node = it->second;
            node->value = value;
            if (policy == Policy::LRU) {
                remove(node);
                addToFront(node);
            }
            return;
        }

        // Case 2: cache is full -> evict the node at the back.
        // LRU: least recently used. FIFO: oldest inserted.
        if ((int)map.size() == capacity) {
            NodeT* victim = tail->prev;
            remove(victim);
            map.erase(victim->key);
            delete victim;
        }

        // Case 3: insert the brand-new node at the front, record it in the map.
        NodeT* node = new NodeT(key, value);
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

// Run a get and print HIT/MISS with the value. Works for any streamable K, V.
template <typename K, typename V>
void tryGet(Cache<K, V>& cache, const K& key) {
    optional<V> v = cache.get(key);
    if (v.has_value())
        cout << "  get(" << key << ")  -> HIT (" << *v << ")\n";
    else
        cout << "  get(" << key << ")  -> MISS\n";
}

// --- Text-file workload harness (integer keys and values) ---
//
// The cache is generic, but a text workload like `put 1 10` is inherently
// integer, so the file harness is specialized to Cache<int, int>.

// One parsed operation. type 'p' = put (uses key+value), 'g' = get (uses key).
// Parsing into a list lets us REPLAY the same workload through several caches.
struct Op {
    char type;
    int key;
    int value;
};

// Execute a parsed workload against one cache, printing each step.
void runOps(Cache<int, int>& cache, const vector<Op>& ops) {
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
// Format: `cap N`, `put KEY VALUE`, `get KEY`. Blank lines and '#' comments are
// ignored. Returns false if the file can't be opened.
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

// Prove the class is generic, not int-only: a tiny DNS cache keyed by strings.
void genericDemo() {
    cout << "\n=== Generic demo: Cache<string, string>, capacity 2 (LRU) ===\n";
    Cache<string, string> dns(2, Policy::LRU, "DNS");
    dns.put("example.com", "93.184.216.34");  cout << "  put(example.com)\n";
    dns.put("openai.com",  "104.18.32.47");   cout << "  put(openai.com)\n";
    tryGet(dns, string("example.com"));                 // HIT -> makes it MRU
    dns.put("anthropic.com", "160.79.104.10");
    cout << "  put(anthropic.com)   [evicts openai.com, the LRU]\n";
    tryGet(dns, string("openai.com"));                  // MISS -> evicted
    tryGet(dns, string("example.com"));                 // HIT -> was protected
    dns.printStats();
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

    Cache<int, int> lru(capacity, Policy::LRU, "LRU");
    cout << "\n=== LRU policy ===\n";
    runOps(lru, ops);
    lru.printStats();

    Cache<int, int> fifo(capacity, Policy::FIFO, "FIFO");
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

    genericDemo();
    return 0;
}
