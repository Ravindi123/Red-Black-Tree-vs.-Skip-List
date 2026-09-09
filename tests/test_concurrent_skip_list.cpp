#include "../include/concurrent_skip_list.hpp"
#include <iostream>
#include <set>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <atomic>

using ds::ConcurrentSkipList;

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, name) do { \
    ++tests_run; \
    if (cond) { ++tests_passed; std::cout << "[PASS] " << name << "\n"; } \
    else { std::cout << "[FAIL] " << name << "\n"; } \
} while (0)

// Single-threaded sanity: same shape of test as the sequential skip list.
void test_single_threaded_basic() {
    ConcurrentSkipList<int> sl(0.5, 16, 1);
    CHECK(sl.insert(10), "insert 10 succeeds");
    CHECK(sl.insert(20), "insert 20 succeeds");
    CHECK(!sl.insert(10), "insert duplicate 10 fails");
    CHECK(sl.contains(10), "contains 10");
    CHECK(sl.contains(20), "contains 20");
    CHECK(!sl.contains(30), "does not contain 30");
    CHECK(sl.size() == 2, "size is 2");

    // Range 0..1999 already contains 10 and 20, so those two re-insertions
    // below are expected (and required, for set semantics) to be no-ops.
    std::mt19937 rng(11);
    std::vector<int> keys;
    for (int i = 0; i < 2000; ++i) keys.push_back(i);
    std::shuffle(keys.begin(), keys.end(), rng);
    for (int k : keys) sl.insert(k);

    auto sorted = sl.to_sorted_vector();
    std::vector<int> expected;
    for (int i = 0; i < 2000; ++i) expected.push_back(i);
    CHECK(sorted == expected, "single-threaded bulk insert produces correct sorted contents");
    CHECK(sl.size() == 2000, "duplicate re-inserts of 10 and 20 were correctly rejected");
}

// The real test: many threads inserting concurrently into the SAME skip
// list, with overlapping key ranges so predecessor locks genuinely contend.
// After joining every thread, the skip list must contain exactly the union
// of everything every thread tried to insert (with duplicates collapsed).
void test_concurrent_insert_disjoint_ranges() {
    const int num_threads = 8;
    const int per_thread = 5000;
    ConcurrentSkipList<int> sl(0.5, 24, 2026);

    std::vector<std::thread> threads;
    std::atomic<int> total_successful_inserts{0};
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            int local_success = 0;
            for (int i = 0; i < per_thread; ++i) {
                int key = t * per_thread + i; // disjoint range per thread
                if (sl.insert(key)) ++local_success;
            }
            total_successful_inserts.fetch_add(local_success);
        });
    }
    for (auto& th : threads) th.join();

    CHECK(total_successful_inserts.load() == num_threads * per_thread,
          "every disjoint-range insert reported success exactly once");
    CHECK(sl.size() == static_cast<size_t>(num_threads * per_thread),
          "final size equals total number of inserted keys");

    auto sorted = sl.to_sorted_vector();
    bool strictly_increasing = true;
    for (size_t i = 1; i < sorted.size(); ++i) {
        if (sorted[i - 1] >= sorted[i]) { strictly_increasing = false; break; }
    }
    CHECK(strictly_increasing, "final contents are strictly increasing (no corruption, no duplicates)");
    CHECK(sorted.size() == static_cast<size_t>(num_threads * per_thread), "sorted output has the right length");
}

// Harder case: many threads racing to insert from the SAME overlapping key
// range, so most attempts collide with a key some other thread also wants.
// Exactly one thread should "win" each key; total successes == unique keys.
void test_concurrent_insert_overlapping_range() {
    const int num_threads = 8;
    const int domain = 5000; // every thread draws from the same [0, domain) range
    const int attempts_per_thread = 4000;
    ConcurrentSkipList<int> sl(0.5, 24, 4);

    std::vector<std::thread> threads;
    std::atomic<int> total_successful_inserts{0};
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            std::mt19937 rng(1000 + t);
            std::uniform_int_distribution<int> dist(0, domain - 1);
            int local_success = 0;
            for (int i = 0; i < attempts_per_thread; ++i) {
                if (sl.insert(dist(rng))) ++local_success;
            }
            total_successful_inserts.fetch_add(local_success);
        });
    }
    for (auto& th : threads) th.join();

    auto sorted = sl.to_sorted_vector();
    std::set<int> unique_check(sorted.begin(), sorted.end());
    CHECK(unique_check.size() == sorted.size(), "no duplicate keys ended up in the structure");
    CHECK(static_cast<int>(sorted.size()) == total_successful_inserts.load(),
          "size matches the number of inserts that reported success");
    CHECK(sorted.size() <= static_cast<size_t>(domain), "size never exceeds the key domain");

    bool strictly_increasing = true;
    for (size_t i = 1; i < sorted.size(); ++i) {
        if (sorted[i - 1] >= sorted[i]) { strictly_increasing = false; break; }
    }
    CHECK(strictly_increasing, "final contents remain strictly increasing under heavy contention");
}

int main() {
    test_single_threaded_basic();
    test_concurrent_insert_disjoint_ranges();
    test_concurrent_insert_overlapping_range();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
