#ifndef SKIP_LIST_HPP
#define SKIP_LIST_HPP

#include <vector>
#include <random>
#include <functional>

namespace ds {

// Randomized alternative to balanced BSTs (Pugh, 1990).
// Set semantics: duplicate keys are rejected on insert.
template <typename Key, typename Compare = std::less<Key>>
class SkipList {
private:
    struct Node {
        Key key;
        std::vector<Node*> forward;
        Node(const Key& k, int level) : key(k), forward(level, nullptr) {}
    };

    Node* head_;
    int max_level_;   // hard cap on levels, chosen for the expected max n
    int level_;        // current highest level in use (1-indexed count)
    double p_;          // promotion probability
    size_t size_;
    Compare comp_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;

    mutable size_t cmp_count_ = 0; // mutable counter for comparisons when insert/search/delete is called
    size_t dynamic_memory_ = 0;

    bool compare(const Key& a, const Key& b) const {
        ++cmp_count_;
        return comp_(a, b);
    }

    bool eq(const Key& a, const Key& b) const {
        return !compare(a, b) && !compare(b, a);
    }

    int random_level() {
        int lvl = 1;
        while (dist_(rng_) < p_ && lvl < max_level_) ++lvl;
        return lvl;
    }

public:
    explicit SkipList(double p = 0.5, int max_level = 32,
                       unsigned seed = std::random_device{}())
        : max_level_(max_level), level_(1), p_(p), size_(0),
          rng_(seed), dist_(0.0, 1.0) {
        head_ = new Node(Key(), max_level_);
    }

    ~SkipList() {
        Node* cur = head_->forward[0];
        while (cur != nullptr) {
            Node* next = cur->forward[0];
            delete cur;
            cur = next;
        }
        delete head_;
    }

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    // Returns false if key already present (set semantics).
    bool insert(const Key& key) {
        std::vector<Node*> update(max_level_, head_);
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->forward[i] != nullptr && compare(cur->forward[i]->key, key)) {
                cur = cur->forward[i];
            }
            update[i] = cur;
        }
        Node* candidate = cur->forward[0];
        if (candidate != nullptr && eq(candidate->key, key)) return false;

        int new_level = random_level();
        if (new_level > level_) {
            for (int i = level_; i < new_level; ++i) update[i] = head_;
            level_ = new_level;
        }
        Node* node = new Node(key, new_level);
        dynamic_memory_ += new_level * sizeof(Node*);
        for (int i = 0; i < new_level; ++i) {
            node->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = node;
        }
        ++size_;
        return true;
    }

    // Returns false if key not found.
    bool remove(const Key& key) {
        std::vector<Node*> update(max_level_, head_);
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->forward[i] != nullptr && compare(cur->forward[i]->key, key)) {
                cur = cur->forward[i];
            }
            update[i] = cur;
        }
        Node* target = cur->forward[0];
        if (target == nullptr || !eq(target->key, key)) return false;

        for (int i = 0; i < level_; ++i) {
            if (update[i]->forward[i] != target) break;
            update[i]->forward[i] = target->forward[i];
        }
        delete target;
        dynamic_memory_ -= target->forward.capacity() * sizeof(Node*);
        while (level_ > 1 && head_->forward[level_ - 1] == nullptr) --level_;
        --size_;
        return true;
    }

    bool search(const Key& key) const {
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (cur->forward[i] != nullptr && compare(cur->forward[i]->key, key)) {
                cur = cur->forward[i];
            }
        }
        cur = cur->forward[0];
        return cur != nullptr && eq(cur->key, key);
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    int current_level() const { return level_; }
    int max_level() const { return max_level_; }
    double promotion_probability() const { return p_; }

    std::vector<Key> to_sorted_vector() const {
        std::vector<Key> out;
        out.reserve(size_);
        for (Node* cur = head_->forward[0]; cur != nullptr; cur = cur->forward[0]) {
            out.push_back(cur->key);
        }
        return out;
    }

    // Sanity checks: level-0 is strictly increasing, and every higher level
    // is a strictly increasing subsequence (structural well-formedness).
    bool validate() const {
        Node* prev = nullptr;
        for (Node* cur = head_->forward[0]; cur != nullptr; cur = cur->forward[0]) {
            if (prev != nullptr && !comp_(prev->key, cur->key)) return false;
            prev = cur;
        }
        for (int i = 1; i < level_; ++i) {
            Node* p = nullptr;
            for (Node* c = head_->forward[i]; c != nullptr; c = c->forward[i]) {
                if (p != nullptr && !comp_(p->key, c->key)) return false;
                p = c;
            }
        }
        return true;
    }

    size_t get_comparisons() const {
        return cmp_count_;
    }

    void reset_comparisons() const {
        cmp_count_ = 0;
    }

    size_t memory_footprint() const {
        size_t base_nodes = (size_ + 1) * sizeof(Node);
        size_t head_vec = max_level_ * sizeof(Node*);
        return sizeof(*this) + base_nodes + head_vec + dynamic_memory_;
    }
};

} // namespace ds

#endif // SKIP_LIST_HPP