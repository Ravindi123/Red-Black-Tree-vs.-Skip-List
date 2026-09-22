#ifndef CONCURRENT_SKIP_LIST_HPP
#define CONCURRENT_SKIP_LIST_HPP

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <new>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

namespace ds {

template <typename Key, typename Compare = std::less<Key>>
class ConcurrentSkipList {
private:
    static constexpr int kMaxSupportedLevel = 96;

    struct Node {
        Key key;
        int top_level;
        std::mutex node_mutex;
        std::atomic<bool> fully_linked;
        bool is_sentinel;

        Node(const Key& k, int level, bool sentinel)
            : key(k), top_level(level), fully_linked(false), is_sentinel(sentinel) {}
    };

    static constexpr size_t header_bytes() {
        return ((sizeof(Node) + alignof(std::atomic<Node*>) - 1) / alignof(std::atomic<Node*>))
               * alignof(std::atomic<Node*>);
    }

    static std::atomic<Node*>* forward_of(Node* n) {
        return reinterpret_cast<std::atomic<Node*>*>(reinterpret_cast<unsigned char*>(n) + header_bytes());
    }

    static Node* make_node(const Key& key, int level, bool sentinel) {
        std::size_t slots = static_cast<std::size_t>(level) + 1;
        std::size_t bytes = header_bytes() + slots * sizeof(std::atomic<Node*>);
        void* mem = ::operator new(bytes);
        Node* n;
        try {
            n = ::new (mem) Node(key, level, sentinel);
        } catch (...) {
            ::operator delete(mem);
            throw;
        }
        std::atomic<Node*>* fwd = forward_of(n);
        for (std::size_t i = 0; i < slots; ++i) {
            ::new (&fwd[i]) std::atomic<Node*>(nullptr);
        }
        return n;
    }

    static void destroy_node(Node* n) {
        std::atomic<Node*>* fwd = forward_of(n);
        std::size_t slots = static_cast<std::size_t>(n->top_level) + 1;
        for (std::size_t i = 0; i < slots; ++i) fwd[i].~atomic();
        n->~Node();
        ::operator delete(n);
    }

    int max_level_;
    double p_;
    Compare comp_;
    Node* head_;
    Node* tail_;
    std::atomic<std::size_t> size_{0};

    bool less_(const Key& a, const Key& b) const { return comp_(a, b); }
    bool eq_(const Key& a, const Key& b) const { return !comp_(a, b) && !comp_(b, a); }

    static std::uint64_t next_thread_seed() {
        static std::atomic<std::uint64_t> counter{0x9E3779B97F4A7C15ull};
        std::uint64_t drawn = counter.fetch_add(0x9E3779B97F4A7C15ull, std::memory_order_relaxed);
        return drawn ^ std::hash<std::thread::id>{}(std::this_thread::get_id());
    }

    int random_level() {
        thread_local std::mt19937_64 tl_rng(next_thread_seed());
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        int lvl = 0;
        while (dist(tl_rng) < p_ && lvl < max_level_ - 1) ++lvl;
        return lvl;
    }

    // Lock-free traversal: fills preds[]/succs[] with, at every level, the
    // last node with key < target and the first node with key >= target.
    // Returns true (with succs[0]->key == key) if the key is already present.
    bool find(const Key& key, Node** preds, Node** succs) const {
        Node* pred = head_;
        for (int level = max_level_ - 1; level >= 0; --level) {
            Node* cur = forward_of(pred)[level].load(std::memory_order_acquire);
            while (cur != tail_ && less_(cur->key, key)) {
                pred = cur;
                cur = forward_of(pred)[level].load(std::memory_order_acquire);
            }
            preds[level] = pred;
            succs[level] = cur;
        }
        return succs[0] != tail_ && eq_(succs[0]->key, key);
    }

public:
    explicit ConcurrentSkipList(double p = 0.5, int max_level = 32, unsigned seed = 42) {
        (void)seed; 
        if (max_level > kMaxSupportedLevel || max_level < 1) {
            throw std::invalid_argument(
                "ConcurrentSkipList: max_level must be in [1, kMaxSupportedLevel]");
        }
        max_level_ = max_level;
        p_ = p;
        // Sentinels carry no meaningful key; head/tail comparisons are
        // avoided entirely by pointer identity, never by key comparison.
        head_ = make_node(Key(), max_level_ - 1, true);
        tail_ = make_node(Key(), max_level_ - 1, true);
        for (int i = 0; i < max_level_; ++i) {
            forward_of(head_)[i].store(tail_, std::memory_order_relaxed);
        }
        head_->fully_linked.store(true, std::memory_order_relaxed);
        tail_->fully_linked.store(true, std::memory_order_relaxed);
    }

    ~ConcurrentSkipList() {
        Node* cur = forward_of(head_)[0].load();
        while (cur != tail_) {
            Node* next = forward_of(cur)[0].load();
            destroy_node(cur);
            cur = next;
        }
        destroy_node(head_);
        destroy_node(tail_);
    }

    ConcurrentSkipList(const ConcurrentSkipList&) = delete;
    ConcurrentSkipList& operator=(const ConcurrentSkipList&) = delete;

    // Thread-safe. Returns false if key already present.
    bool insert(const Key& key) {
        int top_level = random_level();
        Node* preds[kMaxSupportedLevel];
        Node* succs[kMaxSupportedLevel];

        while (true) {
            if (find(key, preds, succs)) {
                return false; // already present (set semantics)
            }

            std::unique_lock<std::mutex> locks[kMaxSupportedLevel];
            int locks_held = 0;
            Node* prev_pred = nullptr;
            bool valid = true;

            // Lock predecessors bottom-up. Lock-coupling in a fixed
            // (bottom-to-top) order across all threads is what prevents
            // deadlock here -- two threads inserting nearby keys always
            // attempt to acquire shared predecessor locks in the same order.
            for (int level = 0; valid && level <= top_level; ++level) {
                Node* pred = preds[level];
                Node* succ = succs[level];
                if (pred != prev_pred) {
                    locks[locks_held++] = std::unique_lock<std::mutex>(pred->node_mutex);
                    prev_pred = pred;
                }
                valid = pred->fully_linked.load(std::memory_order_acquire);
                valid = valid && forward_of(pred)[level].load(std::memory_order_acquire) == succ;
            }

            if (!valid) continue; // predecessor changed under us; retry from scratch

            Node* new_node = make_node(key, top_level, false);
            std::atomic<Node*>* new_fwd = forward_of(new_node);
            for (int level = 0; level <= top_level; ++level) {
                new_fwd[level].store(succs[level], std::memory_order_relaxed);
            }
            for (int level = 0; level <= top_level; ++level) {
                forward_of(preds[level])[level].store(new_node, std::memory_order_release);
            }
            new_node->fully_linked.store(true, std::memory_order_release);
            size_.fetch_add(1, std::memory_order_relaxed);
            return true; // locks released automatically (unique_lock destructors)
        }
    }

    // Thread-safe, lock-free.
    bool contains(const Key& key) const {
        Node* preds[kMaxSupportedLevel];
        Node* succs[kMaxSupportedLevel];
        return find(key, preds, succs) &&
               succs[0]->fully_linked.load(std::memory_order_acquire);
    }

    std::size_t size() const { return size_.load(std::memory_order_relaxed); }

    // Single-threaded diagnostic only: call after all concurrent activity has
    // finished (e.g. after joining all inserting threads).
    std::vector<Key> to_sorted_vector() const {
        std::vector<Key> out;
        out.reserve(size_.load());
        for (Node* cur = forward_of(head_)[0].load(); cur != tail_; cur = forward_of(cur)[0].load()) {
            out.push_back(cur->key);
        }
        return out;
    }
};

} // namespace ds

#endif // CONCURRENT_SKIP_LIST_HPP
