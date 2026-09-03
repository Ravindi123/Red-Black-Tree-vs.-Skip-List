#ifndef RED_BLACK_TREE_HPP
#define RED_BLACK_TREE_HPP

#include <vector>
#include <functional>
#include <algorithm>

namespace ds {

// Deterministic self-balancing BST (Guibas & Sedgewick, 1978).
// Set semantics: duplicate keys are rejected on insert.
template <typename Key, typename Compare = std::less<Key>>
class RedBlackTree {
private:
    enum class Color { RED, BLACK };
    mutable size_t cmp_count_ = 0; // mutable counter for comparisons when insert/search/delete is called

    struct Node {
        Key key;
        Color color;
        Node* left;
        Node* right;
        Node* parent;
        Node(const Key& k, Color c, Node* nil)
            : key(k), color(c), left(nil), right(nil), parent(nil) {}
    };

    Node* nil_;   // sentinel leaf, always BLACK, self-referential
    Node* root_;
    size_t size_;
    Compare comp_;

    bool compare(const Key& a, const Key& b) const {
        ++cmp_count_;
        return comp_(a, b);
    }

    bool eq(const Key& a, const Key& b) const {
        return !compare(a, b) && !compare(b, a);
    }

    void left_rotate(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != nil_) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_) root_ = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;
        y->left = x;
        x->parent = y;
    }

    void right_rotate(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right != nil_) y->right->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_) root_ = y;
        else if (x == x->parent->right) x->parent->right = y;
        else x->parent->left = y;
        y->right = x;
        x->parent = y;
    }

    void insert_fixup(Node* z) {
        while (z->parent->color == Color::RED) {
            if (z->parent == z->parent->parent->left) {
                Node* y = z->parent->parent->right;
                if (y->color == Color::RED) {
                    z->parent->color = Color::BLACK;
                    y->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        z = z->parent;
                        left_rotate(z);
                    }
                    z->parent->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    right_rotate(z->parent->parent);
                }
            } else {
                Node* y = z->parent->parent->left;
                if (y->color == Color::RED) {
                    z->parent->color = Color::BLACK;
                    y->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        right_rotate(z);
                    }
                    z->parent->color = Color::BLACK;
                    z->parent->parent->color = Color::RED;
                    left_rotate(z->parent->parent);
                }
            }
        }
        root_->color = Color::BLACK;
    }

    void transplant(Node* u, Node* v) {
        if (u->parent == nil_) root_ = v;
        else if (u == u->parent->left) u->parent->left = v;
        else u->parent->right = v;
        v->parent = u->parent;
    }

    Node* minimum(Node* x) const {
        while (x->left != nil_) x = x->left;
        return x;
    }

    void delete_fixup(Node* x) {
        while (x != root_ && x->color == Color::BLACK) {
            if (x == x->parent->left) {
                Node* w = x->parent->right;
                if (w->color == Color::RED) {
                    w->color = Color::BLACK;
                    x->parent->color = Color::RED;
                    left_rotate(x->parent);
                    w = x->parent->right;
                }
                if (w->left->color == Color::BLACK && w->right->color == Color::BLACK) {
                    w->color = Color::RED;
                    x = x->parent;
                } else {
                    if (w->right->color == Color::BLACK) {
                        w->left->color = Color::BLACK;
                        w->color = Color::RED;
                        right_rotate(w);
                        w = x->parent->right;
                    }
                    w->color = x->parent->color;
                    x->parent->color = Color::BLACK;
                    w->right->color = Color::BLACK;
                    left_rotate(x->parent);
                    x = root_;
                }
            } else {
                Node* w = x->parent->left;
                if (w->color == Color::RED) {
                    w->color = Color::BLACK;
                    x->parent->color = Color::RED;
                    right_rotate(x->parent);
                    w = x->parent->left;
                }
                if (w->right->color == Color::BLACK && w->left->color == Color::BLACK) {
                    w->color = Color::RED;
                    x = x->parent;
                } else {
                    if (w->left->color == Color::BLACK) {
                        w->right->color = Color::BLACK;
                        w->color = Color::RED;
                        left_rotate(w);
                        w = x->parent->left;
                    }
                    w->color = x->parent->color;
                    x->parent->color = Color::BLACK;
                    w->left->color = Color::BLACK;
                    right_rotate(x->parent);
                    x = root_;
                }
            }
        }
        x->color = Color::BLACK;
    }

    Node* find_node(const Key& key) const {
        Node* x = root_;
        while (x != nil_ && !eq(x->key, key)) {
            x = compare(key, x->key) ? x->left : x->right;
        }
        return x;
    }

    void destroy(Node* x) {
        if (x == nil_) return;
        destroy(x->left);
        destroy(x->right);
        delete x;
    }

    void inorder_collect(Node* x, std::vector<Key>& out) const {
        if (x == nil_) return;
        inorder_collect(x->left, out);
        out.push_back(x->key);
        inorder_collect(x->right, out);
    }

    int height_of(Node* x) const {
        if (x == nil_) return 0;
        return 1 + std::max(height_of(x->left), height_of(x->right));
    }

    // Recursively checks RB invariants + BST ordering; returns black-height.
    int validate_node(Node* x, bool& ok) const {
        if (x == nil_) return 1;
        if (x->color == Color::RED &&
            (x->left->color == Color::RED || x->right->color == Color::RED)) {
            ok = false;
        }
        int lh = validate_node(x->left, ok);
        int rh = validate_node(x->right, ok);
        if (lh != rh) ok = false;
        if (x->left != nil_ && compare(x->key, x->left->key)) ok = false;
        if (x->right != nil_ && compare(x->right->key, x->key)) ok = false;
        return lh + (x->color == Color::BLACK ? 1 : 0);
    }

public:
    RedBlackTree() : size_(0) {
        nil_ = new Node(Key(), Color::BLACK, nullptr);
        nil_->left = nil_->right = nil_->parent = nil_;
        root_ = nil_;
    }

    ~RedBlackTree() {
        destroy(root_);
        delete nil_;
    }

    RedBlackTree(const RedBlackTree&) = delete;
    RedBlackTree& operator=(const RedBlackTree&) = delete;

    // Returns false if key already present (set semantics).
    bool insert(const Key& key) {
        Node* y = nil_;
        Node* x = root_;
        while (x != nil_) {
            y = x;
            if (eq(key, x->key)) return false;
            x = compare(key, x->key) ? x->left : x->right;
        }
        Node* z = new Node(key, Color::RED, nil_);
        z->parent = y;
        if (y == nil_) root_ = z;
        else if (compare(z->key, y->key)) y->left = z;
        else y->right = z;
        insert_fixup(z);
        ++size_;
        return true;
    }

    // Returns false if key not found.
    bool remove(const Key& key) {
        Node* z = find_node(key);
        if (z == nil_) return false;
        Node* y = z;
        Color y_original_color = y->color;
        Node* x;
        if (z->left == nil_) {
            x = z->right;
            transplant(z, z->right);
        } else if (z->right == nil_) {
            x = z->left;
            transplant(z, z->left);
        } else {
            y = minimum(z->right);
            y_original_color = y->color;
            x = y->right;
            if (y->parent == z) {
                x->parent = y;
            } else {
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }
        delete z;
        if (y_original_color == Color::BLACK) delete_fixup(x);
        --size_;
        return true;
    }

    bool search(const Key& key) const { return find_node(key) != nil_; }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    int height() const { return height_of(root_); }

    std::vector<Key> inorder() const {
        std::vector<Key> out;
        out.reserve(size_);
        inorder_collect(root_, out);
        return out;
    }

    // Checks: root black, no red-red violation, equal black-height on every
    // path, and BST ordering. Used by tests and, later, benchmark sanity checks.
    bool validate() const {
        if (root_->color != Color::BLACK) return false;
        bool ok = true;
        validate_node(root_, ok);
        return ok;
    }
    
    size_t get_comparisons() const {
        return cmp_count_;
    } 

    void reset_comparisons() const {
        cmp_count_ = 0;
    }

    size_t memory_footprint() const {
        return sizeof(*this) + (size_ + 1) * sizeof(Node);
    }
};

}// namespace ds

#endif // RED_BLACK_TREE_HPP