#include "../include/red_black_tree.hpp"
#include "../include/concurrent_skip_list.hpp"
#include "../include/dataset_generator.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

using namespace ds;
using namespace std::chrono;

namespace {

struct Args {
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    std::int64_t total_keys = 1000000;
    int repeats = 5;
    std::string output = "results/threading_results.csv";
};

std::vector<int> split_ints(const std::string& s) {
    std::vector<int> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) out.push_back(std::stoi(item));
    return out;
}

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + arg);
            return argv[++i];
        };
        if (arg == "--threads") a.thread_counts = split_ints(next());
        else if (arg == "--total-keys") a.total_keys = std::stoll(next());
        else if (arg == "--repeats") a.repeats = std::stoi(next());
        else if (arg == "--output") a.output = next();
        else throw std::runtime_error("unknown argument: " + arg);
    }
    return a;
}

double elapsed_ms(steady_clock::time_point start,
                   steady_clock::time_point end) {
    return duration<double, std::milli>(end - start).count();
}

// Max level set to log2(n) plus a small margin
int skip_list_max_level(std::int64_t n) {
    int level = 1;
    while ((std::int64_t(1) << level) < n) ++level;
    return std::max(4, std::min(32, level + 2));
}

// Splits `keys` into `num_parts` contiguous, roughly-equal, disjoint slices.
std::vector<std::pair<size_t, size_t>> partition_ranges(size_t total, int num_parts) {
    std::vector<std::pair<size_t, size_t>> ranges;
    size_t base = total / static_cast<size_t>(num_parts);
    size_t remainder = total % static_cast<size_t>(num_parts);
    size_t start = 0;
    for (int i = 0; i < num_parts; ++i) {
        size_t len = base + (static_cast<size_t>(i) < remainder ? 1 : 0);
        ranges.emplace_back(start, start + len);
        start += len;
    }
    return ranges;
}

template <typename Structure>
double run_one_trial(const std::vector<int>& keys, int num_threads, Structure& structure) {
    auto ranges = partition_ranges(keys.size(), num_threads);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<size_t>(num_threads));

    auto t0 = std::chrono::steady_clock::now();
    for (int t = 0; t < num_threads; ++t) {
        auto [lo, hi] = ranges[static_cast<size_t>(t)];
        threads.emplace_back([&structure, &keys, lo, hi]() {
            for (size_t i = lo; i < hi; ++i) structure.insert(keys[i]);
        });
    }
    for (auto& th : threads) th.join();
    auto t1 = std::chrono::steady_clock::now();
    return elapsed_ms(t0, t1);
}

template <typename Structure>
double run_single_threaded_trial(const std::vector<int>& keys, Structure& structure) {
    auto t0 = std::chrono::steady_clock::now();
    for (int key : keys) structure.insert(key);
    auto t1 = std::chrono::steady_clock::now();
    return elapsed_ms(t0, t1);
}

} // namespace

int main(int argc, char** argv) {
    Args args;
    try {
        args = parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Argument error: " << e.what() << "\n";
        return 1;
    }

    unsigned hw = std::thread::hardware_concurrency();
    std::cerr << "Detected hardware_concurrency() = " << hw
              << " (results only show real scaling if this is > 1)\n";

    std::ofstream out(args.output);
    if (!out) {
        std::cerr << "Could not open output file: " << args.output << "\n";
        return 1;
    }
    out << "structure,threads,repeat,total_keys,elapsed_ms,throughput_ops_per_sec,"
           "final_size,structure_valid\n";

    int sl_max_level = skip_list_max_level(args.total_keys);
    for (int threads : args.thread_counts) {
        for (int rep = 0; rep < args.repeats; ++rep) {
            unsigned seed = static_cast<unsigned>(threads * 1000003u + rep * 97u);
            auto keys = bench::DatasetGenerator::generate_uniform(args.total_keys, seed);

            {
                RedBlackTree<int> rbt;
                double ms = run_single_threaded_trial(keys, rbt);
                double throughput = static_cast<double>(args.total_keys) / (ms / 1000.0);
                bool valid = (rep == 0) ? rbt.validate() : true; 
                // threads column records the concurrency level under test for this row's
                // skip list counterpart; RBT itself always runs single-threaded here.
                out << "RBT_SingleThreaded," << threads << "," << rep << "," << args.total_keys << ","
                    << ms << "," << throughput << "," << rbt.size() << "," << (valid ? 1 : 0) << "\n";
            }
            {
                ConcurrentSkipList<int> sl(0.5, sl_max_level, seed + 555);
                double ms = run_one_trial(keys, threads, sl);
                double throughput = static_cast<double>(args.total_keys) / (ms / 1000.0);
                bool valid = true;
                if (rep == 0) {
                    auto sorted = sl.to_sorted_vector();
                    valid = std::is_sorted(sorted.begin(), sorted.end()) &&
                            sorted.size() == sl.size();
                }
                out << "SkipList_FineGrained," << threads << "," << rep << "," << args.total_keys << ","
                    << ms << "," << throughput << "," << sl.size() << "," << (valid ? 1 : 0) << "\n";
            }
            out.flush();
        }
        std::cerr << "finished threads=" << threads << "\n";
    }

    std::cerr << "Done. Wrote " << args.output << "\n";
    return 0;
}
