#ifndef SKIP_LIST_HPP
#define SKIP_LIST_HPP

#include <vector>
#include <random>
#include <functional>
#include <stdexcept>
#include <new>
#include <cstddef>

namespace ds {

// Set semantics: duplicate keys are rejected on insert.
template <typename Key, typename Compare = std::less<Key>>
class SkipList {
private:
    
    static constexpr int kMaxSupportedLevel = 96;

    // Each node's forward array is allocated as extra bytes immediately 
    // after this header in the same heap block, so constructing
    // a node costs exactly one allocation
    struct Node {
        Key key;
        int level;
    };

    static constexpr size_t header_bytes() {
        return ((sizeof(Node) + alignof(Node*) - 1) / alignof(Node*)) * alignof(Node*);
    }

    static Node** forward_of(Node* n) {
        return reinterpret_cast<Node**>(reinterpret_cast<unsigned char*>(n) + header_bytes());
    }

    static Node* make_node(const Key& key, int level) {
        std::size_t bytes = header_bytes() + static_cast<std::size_t>(level) * sizeof(Node*);
        void* mem = ::operator new(bytes);
        Node* n;
        try {
            n = ::new (mem) Node{key, level};
        } catch (...) {
            ::operator delete(mem);
            throw;
        }
        Node** fwd = forward_of(n);
        for (int i = 0; i < level; ++i) fwd[i] = nullptr;
        return n;
    }

    static void destroy_node(Node* n) {
        n->~Node();
        ::operator delete(n);
    }

    Node* head_;
    int max_level_;   // hard cap on levels, chosen for the expected max n
    int level_;        // current highest level in use (1-indexed count)
    double p_;          // promotion probability
    size_t size_;
    Compare comp_;
    // std::mt19937 rng_;
    // std::uniform_real_distribution<double> dist_;
    
    mutable size_t cmp_count_ = 0; // mutable counter for comparisons when insert/search/delete is called
    size_t dynamic_memory_ = 0; 

    uint32_t fast_rng_state_;
    uint32_t promotion_threshold_;

    uint32_t xorshift32() {
        fast_rng_state_ ^= fast_rng_state_ << 13;
        fast_rng_state_ ^= fast_rng_state_ >> 17;
        fast_rng_state_ ^= fast_rng_state_ << 5;
        return fast_rng_state_;
    }

     bool compare(const Key& a, const Key& b) const {
        ++cmp_count_;
        return comp_(a, b);
    }

    bool eq(const Key& a, const Key& b) const {
        return !compare(a, b) && !compare(b, a);
    }

    // int random_level() {
    //     int lvl = 1;
    //     while (dist_(rng_) < p_ && lvl < max_level_) ++lvl;
    //     return lvl;
    // }

    int random_level() {
        int lvl = 1;
        while (xorshift32() < promotion_threshold_ && lvl < max_level_) ++lvl;
        return lvl;
    }

public:
    explicit SkipList(double p = 0.5, int max_level = 32,
                       unsigned seed = std::random_device{}())
        : max_level_(max_level), level_(1), p_(p), size_(0),
          fast_rng_state_(seed | 1) {
        if (max_level_ > kMaxSupportedLevel || max_level_ < 1) {
            throw std::invalid_argument(
                "SkipList: max_level must be in [1, kMaxSupportedLevel]");
        }

        promotion_threshold_ = static_cast<uint32_t>(p_ * 0xFFFFFFFF);

        head_ = make_node(Key(), max_level_);
    }

    ~SkipList() {
        Node* cur = forward_of(head_)[0];
        while (cur != nullptr) {
            Node* next = forward_of(cur)[0];
            destroy_node(cur);
            cur = next;
        }
        destroy_node(head_);
    }

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    // Returns false if key already present (set semantics).
    bool insert(const Key& key) {
        // Uses a fixed-size stack array for the update path to avoid dynamic memory allocation.
        Node* update[kMaxSupportedLevel];
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (forward_of(cur)[i] != nullptr && compare(forward_of(cur)[i]->key, key)) {
                cur = forward_of(cur)[i];
            }
            update[i] = cur;
        }
        Node* candidate = forward_of(cur)[0];
        if (candidate != nullptr && eq(candidate->key, key)) return false;

        int new_level = random_level();
        if (new_level > level_) {
            for (int i = level_; i < new_level; ++i) update[i] = head_;
            level_ = new_level;
        }
        Node* node = make_node(key, new_level);
        Node** node_fwd = forward_of(node);
        for (int i = 0; i < new_level; ++i) {
            node_fwd[i] = forward_of(update[i])[i];
            forward_of(update[i])[i] = node;
        }
        dynamic_memory_ += static_cast<size_t>(new_level) * sizeof(Node*);
        ++size_;
        return true;
    }

    // Returns false if key not found.
    bool remove(const Key& key) {
        Node* update[kMaxSupportedLevel];
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (forward_of(cur)[i] != nullptr && compare(forward_of(cur)[i]->key, key)) {
                cur = forward_of(cur)[i];
            }
            update[i] = cur;
        }
        Node* target = forward_of(cur)[0];
        if (target == nullptr || !eq(target->key, key)) return false;

        for (int i = 0; i < level_; ++i) {
            if (forward_of(update[i])[i] != target) break;
            forward_of(update[i])[i] = forward_of(target)[i];
        }
        dynamic_memory_ -= static_cast<size_t>(target->level) * sizeof(Node*);
        destroy_node(target);
        while (level_ > 1 && forward_of(head_)[level_ - 1] == nullptr) --level_;
        --size_;
        return true;
    }

    bool search(const Key& key) const {
        Node* cur = head_;
        for (int i = level_ - 1; i >= 0; --i) {
            while (forward_of(cur)[i] != nullptr && compare(forward_of(cur)[i]->key, key)) {
                cur = forward_of(cur)[i];
            }
        }
        cur = forward_of(cur)[0];
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
        for (Node* cur = forward_of(head_)[0]; cur != nullptr; cur = forward_of(cur)[0]) {
            out.push_back(cur->key);
        }
        return out;
    }

    // Sanity checks: level-0 is strictly increasing, and every higher level
    // is a strictly increasing subsequence.
    bool validate() const {
        Node* prev = nullptr;
        for (Node* cur = forward_of(head_)[0]; cur != nullptr; cur = forward_of(cur)[0]) {
            if (prev != nullptr && !comp_(prev->key, cur->key)) return false;
            prev = cur;
        }
        for (int i = 1; i < level_; ++i) {
            Node* p = nullptr;
            for (Node* c = forward_of(head_)[i]; c != nullptr; c = forward_of(c)[i]) {
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
