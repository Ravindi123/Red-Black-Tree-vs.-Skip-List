#include "skip_list.hpp"
#include <iostream>
#include <set>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>

using ds::SkipList;

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, name) do { \
    ++tests_run; \
    if (cond) { ++tests_passed; std::cout << "[PASS] " << name << "\n"; } \
    else { std::cout << "[FAIL] " << name << "\n"; } \
} while (0)

void test_basic_insert_search() {
    SkipList<int> sl(0.5, 16, 1);
    CHECK(sl.insert(10), "insert 10 succeeds");
    CHECK(sl.insert(20), "insert 20 succeeds");
    CHECK(!sl.insert(10), "insert duplicate 10 fails");
    CHECK(sl.search(10), "search 10 found");
    CHECK(sl.search(20), "search 20 found");
    CHECK(!sl.search(30), "search 30 not found");
    CHECK(sl.size() == 2, "size is 2");
    CHECK(sl.validate(), "skip list valid after basic inserts");
}

void test_delete() {
    SkipList<int> sl(0.5, 16, 2);
    std::vector<int> keys = {50, 30, 70, 20, 40, 60, 80};
    for (int k : keys) sl.insert(k);
    CHECK(sl.validate(), "skip list valid after inserts");

    CHECK(sl.remove(20), "remove 20");
    CHECK(!sl.search(20), "20 gone after remove");
    CHECK(sl.validate(), "skip list valid after removal");

    CHECK(!sl.remove(999), "removing missing key fails");
}

void test_sorted_order() {
    SkipList<int> sl(0.5, 16, 3);
    std::vector<int> keys = {5, 3, 8, 1, 4, 7, 9, 2, 6, 0};
    for (int k : keys) sl.insert(k);
    auto sorted_keys = keys;
    std::sort(sorted_keys.begin(), sorted_keys.end());
    CHECK(sl.to_sorted_vector() == sorted_keys, "sorted traversal matches expected order");
}

// Randomized differential test: apply the same op sequence to the skip list
// and to std::set (a trusted oracle), and check they always agree.
void test_stress_against_std_set() {
    std::mt19937 rng(7);
    std::uniform_int_distribution<int> key_dist(0, 20000);
    std::uniform_int_distribution<int> op_dist(0, 2);

    SkipList<int> sl(0.5, 20, 99);
    std::set<int> oracle;

    const int N = 20000;
    bool all_ok = true;
    for (int i = 0; i < N; ++i) {
        int op = op_dist(rng);
        int key = key_dist(rng);
        if (op == 0) {
            if (sl.insert(key) != oracle.insert(key).second) all_ok = false;
        } else if (op == 1) {
            if (sl.remove(key) != (oracle.erase(key) > 0)) all_ok = false;
        } else {
            if (sl.search(key) != (oracle.find(key) != oracle.end())) all_ok = false;
        }
        if (i % 2000 == 0 && !sl.validate()) all_ok = false;
    }
    CHECK(all_ok, "stress test matches std::set behaviour");
    CHECK(sl.validate(), "skip list valid after stress test");
    CHECK(sl.size() == oracle.size(), "size matches std::set size after stress");

    std::vector<int> expected(oracle.begin(), oracle.end());
    CHECK(sl.to_sorted_vector() == expected, "final sorted order matches std::set contents");
}

void test_expected_height_matches_theory() {
    // Pugh's expected max level for n elements with promotion p is
    // roughly log_{1/p}(n). Single-run slack is generous on purpose --
    // the proposal's real p-sensitivity study happens in phase 2 (30 runs/config).
    SkipList<int> sl(0.5, 32, 123);
    const int N = 100000;
    for (int i = 0; i < N; ++i) sl.insert(i);
    double expected = std::log((double)N) / std::log(1.0 / 0.5);
    bool within_range = sl.current_level() < expected + 15;
    CHECK(within_range, "observed level roughly tracks theoretical log_{1/p}(n)");
    std::cout << "    (observed level = " << sl.current_level()
              << ", theoretical expected ~= " << expected << ")\n";
}

int main() {
    test_basic_insert_search();
    test_delete();
    test_sorted_order();
    test_stress_against_std_set();
    test_expected_height_matches_theory();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return (tests_passed == tests_run) ? 0 : 1;
}