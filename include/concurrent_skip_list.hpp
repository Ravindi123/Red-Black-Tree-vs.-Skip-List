#ifndef CONCURRENT_SKIP_LIST_HPP
#define CONCURRENT_SKIP_LIST_HPP

#include <atomic>
#include <functional>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

namespace ds {

template <typename Key, typename Compare = std::less<Key>>
class ConcurrentSkipList {
private:
    struct Node {
        Key key;
        int top_level;
        std::vector<std::atomic<Node*>> next;
        std::mutex node_mutex;
        std::atomic<bool> fully_linked{false};
        bool is_sentinel;

        Node(const Key& k, int level, bool sentinel)
            : key(k), top_level(level), next(static_cast<size_t>(level) + 1), is_sentinel(sentinel) {
            for (auto& p : next) p.store(nullptr, std::memory_order_relaxed);
        }
    };

    int max_level_;
    double p_;
    Compare comp_;
    Node* head_;
    Node* tail_;
    std::mt19937_64 rng_;
    std::mutex rng_mutex_; // random_level() is called concurrently; guard the shared engine
    std::atomic<std::size_t> size_{0};

    bool less_(const Key& a, const Key& b) const { return comp_(a, b); }
    bool eq_(const Key& a, const Key& b) const { return !comp_(a, b) && !comp_(b, a); }

    int random_level() {
        std::lock_guard<std::mutex> lock(rng_mutex_);
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        int lvl = 0;
        while (dist(rng_) < p_ && lvl < max_level_ - 1) ++lvl;
        return lvl;
    }

    // Lock-free traversal: fills preds[]/succs[] with, at every level, the
    // last node with key < target and the first node with key >= target.
    // Returns true (with succs[0]->key == key) if the key is already present.
    bool find(const Key& key, std::vector<Node*>& preds, std::vector<Node*>& succs) const {
        Node* pred = head_;
        for (int level = max_level_ - 1; level >= 0; --level) {
            Node* cur = pred->next[level].load(std::memory_order_acquire);
            while (cur != tail_ && less_(cur->key, key)) {
                pred = cur;
                cur = pred->next[level].load(std::memory_order_acquire);
            }
            preds[level] = pred;
            succs[level] = cur;
        }
        return succs[0] != tail_ && eq_(succs[0]->key, key);
    }

public:
    explicit ConcurrentSkipList(double p = 0.5, int max_level = 32, unsigned seed = 42)
        : max_level_(max_level), p_(p), rng_(seed) {
        // Sentinels carry no meaningful key; head/tail comparisons are
        // avoided entirely by pointer identity, never by key comparison.
        head_ = new Node(Key(), max_level_ - 1, true);
        tail_ = new Node(Key(), max_level_ - 1, true);
        for (int i = 0; i < max_level_; ++i) {
            head_->next[static_cast<size_t>(i)].store(tail_, std::memory_order_relaxed);
        }
        head_->fully_linked.store(true, std::memory_order_relaxed);
        tail_->fully_linked.store(true, std::memory_order_relaxed);
    }

    ~ConcurrentSkipList() {
        Node* cur = head_->next[0].load();
        while (cur != tail_) {
            Node* next = cur->next[0].load();
            delete cur;
            cur = next;
        }
        delete head_;
        delete tail_;
    }

    ConcurrentSkipList(const ConcurrentSkipList&) = delete;
    ConcurrentSkipList& operator=(const ConcurrentSkipList&) = delete;

    // Thread-safe. Returns false if key already present.
    bool insert(const Key& key) {
        int top_level = random_level();
        std::vector<Node*> preds(static_cast<size_t>(max_level_));
        std::vector<Node*> succs(static_cast<size_t>(max_level_));

        while (true) {
            if (find(key, preds, succs)) {
                return false; // already present (set semantics)
            }

            std::vector<std::unique_lock<std::mutex>> locks;
            locks.reserve(static_cast<size_t>(top_level) + 1);
            Node* prev_pred = nullptr;
            bool valid = true;

            // Lock predecessors bottom-up. Lock-coupling in a fixed
            // (bottom-to-top) order across all threads is what prevents
            // deadlock here -- two threads inserting nearby keys always
            // attempt to acquire shared predecessor locks in the same order.
            for (int level = 0; valid && level <= top_level; ++level) {
                Node* pred = preds[static_cast<size_t>(level)];
                Node* succ = succs[static_cast<size_t>(level)];
                if (pred != prev_pred) {
                    locks.emplace_back(pred->node_mutex);
                    prev_pred = pred;
                }
                valid = pred->fully_linked.load(std::memory_order_acquire);
                valid = valid &&
                        pred->next[static_cast<size_t>(level)].load(std::memory_order_acquire) == succ;
            }

            if (!valid) continue; // predecessor changed under us; retry from scratch

            Node* new_node = new Node(key, top_level, false);
            for (int level = 0; level <= top_level; ++level) {
                new_node->next[static_cast<size_t>(level)].store(
                    succs[static_cast<size_t>(level)], std::memory_order_relaxed);
            }
            for (int level = 0; level <= top_level; ++level) {
                preds[static_cast<size_t>(level)]
                    ->next[static_cast<size_t>(level)]
                    .store(new_node, std::memory_order_release);
            }
            new_node->fully_linked.store(true, std::memory_order_release);
            size_.fetch_add(1, std::memory_order_relaxed);
            return true; // locks released automatically (unique_lock destructors)
        }
    }

    // Thread-safe, lock-free.
    bool contains(const Key& key) const {
        std::vector<Node*> preds(static_cast<size_t>(max_level_));
        std::vector<Node*> succs(static_cast<size_t>(max_level_));
        return find(key, preds, succs) &&
               succs[0]->fully_linked.load(std::memory_order_acquire);
    }

    std::size_t size() const { return size_.load(std::memory_order_relaxed); }

    // Single-threaded diagnostic only: call after all concurrent activity has
    // finished (e.g. after joining all inserting threads).
    std::vector<Key> to_sorted_vector() const {
        std::vector<Key> out;
        out.reserve(size_.load());
        for (Node* cur = head_->next[0].load(); cur != tail_; cur = cur->next[0].load()) {
            out.push_back(cur->key);
        }
        return out;
    }
};

} // namespace ds

#endif // CONCURRENT_SKIP_LIST_HPP
