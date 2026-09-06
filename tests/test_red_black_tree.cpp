#include "red_black_tree.hpp"
#include <iostream>
#include <set>
#include <vector>
#include <random>
#include <algorithm>

using ds::RedBlackTree;

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, name) do { \
    ++tests_run; \
    if (cond) { ++tests_passed; std::cout << "[PASS] " << name << "\n"; } \
    else { std::cout << "[FAIL] " << name << "\n"; } \
} while (0)

void test_basic_insert_search() {
    RedBlackTree<int> t;
    CHECK(t.insert(10), "insert 10 succeeds");
    CHECK(t.insert(20), "insert 20 succeeds");
    CHECK(!t.insert(10), "insert duplicate 10 fails");
    CHECK(t.search(10), "search 10 found");
    CHECK(t.search(20), "search 20 found");
    CHECK(!t.search(30), "search 30 not found");
    CHECK(t.size() == 2, "size is 2");
    CHECK(t.validate(), "tree valid after basic inserts");
}

void test_delete() {
    RedBlackTree<int> t;
    std::vector<int> keys = {50, 30, 70, 20, 40, 60, 80};
    for (int k : keys) t.insert(k);
    CHECK(t.validate(), "tree valid after inserts");

    CHECK(t.remove(20), "remove leaf 20");
    CHECK(!t.search(20), "20 gone after remove");
    CHECK(t.validate(), "tree valid after leaf removal");

    CHECK(t.remove(50), "remove root 50");
    CHECK(!t.search(50), "50 gone after remove");
    CHECK(t.validate(), "tree valid after root removal");

    CHECK(!t.remove(999), "removing missing key fails");
}

void test_inorder_matches_sorted() {
    RedBlackTree<int> t;
    std::vector<int> keys = {5, 3, 8, 1, 4, 7, 9, 2, 6, 0};
    for (int k : keys) t.insert(k);
    auto sorted_keys = keys;
    std::sort(sorted_keys.begin(), sorted_keys.end());
    CHECK(t.inorder() == sorted_keys, "inorder traversal is sorted");
}

// Randomized differential test: apply the same op sequence to the RB tree
// and to std::set (a trusted oracle), and check they always agree.
void test_stress_against_std_set() {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> key_dist(0, 20000);
    std::uniform_int_distribution<int> op_dist(0, 2); // 0=insert,1=remove,2=search

    RedBlackTree<int> t;
    std::set<int> oracle;

    const int N = 20000;
    bool all_ok = true;
    for (int i = 0; i < N; ++i) {
        int op = op_dist(rng);
        int key = key_dist(rng);
        if (op == 0) {
            if (t.insert(key) != oracle.insert(key).second) all_ok = false;
        } else if (op == 1) {
            if (t.remove(key) != (oracle.erase(key) > 0)) all_ok = false;
        } else {
            if (t.search(key) != (oracle.find(key) != oracle.end())) all_ok = false;
        }
        if (i % 2000 == 0 && !t.validate()) all_ok = false;
    }
    CHECK(all_ok, "stress test matches std::set behaviour");
    CHECK(t.validate(), "tree valid after stress test");
    CHECK(t.size() == oracle.size(), "size matches std::set size after stress");

    std::vector<int> expected(oracle.begin(), oracle.end());
    CHECK(t.inorder() == expected, "final inorder matches std::set contents");
}

int main() {
    test_basic_insert_search();
    test_delete();
    test_inorder_matches_sorted();
    test_stress_against_std_set();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return (tests_passed == tests_run) ? 0 : 1;
}