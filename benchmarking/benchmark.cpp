#include "../include/skip_list_optimized.hpp"
#include "../include/red_black_tree.hpp"
#include "../include/dataset_generators.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <random>
#include <type_traits>

using namespace ds;
using namespace std::chrono;

template <typename Structure, typename Key>
void run_workload(const std::string& struct_name, const std::string& dataset_name, 
                  const std::vector<Key>& keys, int run, std::ofstream& file) {
    
    Structure ds;
    size_t n = keys.size();
    
    // 1. Insertion Phase
    ds.reset_comparisons();
    auto start = steady_clock::now();
    for (const Key& key : keys) ds.insert(key);
    auto end = steady_clock::now();
    
    double insert_ms = duration_cast<duration<double, std::milli>>(end - start).count();
    
    int current_height = 0;
    if constexpr (std::is_same_v<Structure, RedBlackTree<Key>>) current_height = ds.height();
    else current_height = ds.current_level();

    // Write directly to file and flush to save immediately
    file << struct_name << "," << dataset_name << "," << n << "," << run << ","
         << "Insert," << insert_ms << "," << ds.get_comparisons() << "," 
         << ds.memory_footprint() << "," << current_height << "\n";
    file.flush();

    // 2. Search Phase 
    std::vector<Key> search_keys = keys;
    std::mt19937 rng(run);
    std::shuffle(search_keys.begin(), search_keys.end(), rng);
    // Turn ~10% of the lookups into guaranteed misses.
    for(size_t i = 0; i < search_keys.size() / 10; ++i) {
        if constexpr (std::is_same_v<Key, std::string>) search_keys[i] += "_MISS";
        else search_keys[i] = -search_keys[i];
    }

    ds.reset_comparisons();
    start = steady_clock::now();
    for (const Key& key : search_keys) ds.search(key);
    end = steady_clock::now();
    
    double search_ms = duration_cast<duration<double, std::milli>>(end - start).count();
    
    file << struct_name << "," << dataset_name << "," << n << "," << run << ","
         << "Search," << search_ms << "," << ds.get_comparisons() << "," 
         << ds.memory_footprint() << "," << current_height << "\n";
    file.flush();

    // 3. Deletion Phase
    std::vector<Key> delete_keys = keys;
    std::shuffle(delete_keys.begin(), delete_keys.end(), rng);
    
    ds.reset_comparisons();
    start = steady_clock::now();
    for (const Key& key : delete_keys) ds.remove(key);
    end = steady_clock::now();
    
    double delete_ms = duration_cast<duration<double, std::milli>>(end - start).count();
    
    file << struct_name << "," << dataset_name << "," << n << "," << run << ","
         << "Delete," << delete_ms << "," << ds.get_comparisons() << "," 
         << ds.memory_footprint() << ",0\n";
    file.flush();
}

int main() {
    std::vector<size_t> N_values = {1000, 10000, 100000, 1000000}; 
    int num_runs = 5; 
    
    std::string filename = "results/benchmark_results_optimized.csv";
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not create file " << filename << "\n";
        return 1;
    }

    std::vector<std::string> real_keys;
    try {
        real_keys = bench::load_real_world("datasets/google-10000-english-usa.txt");
    } catch (const std::exception& e) {
        std::cerr << "Warning: could not load real dataset: " << e.what() << "\n";
    }

    std::cout << "Starting benchmarks. Writing to " << filename << "...\n";
    file << "Structure,Dataset,N,Run,Operation,Time_ms,Comparisons,Memory_bytes,Height\n";
    file.flush();

    for (size_t n : N_values) {
        std::cout << "Testing N = " << n << "\n";

        auto uniform = bench::generate_uniform(n, 42);
        auto sorted = bench::generate_sorted(n);
        auto skewed = bench::generate_skewed(n, 42);

        for (int run = 1; run <= num_runs; ++run) {
            
            run_workload<RedBlackTree<bench::Key>>("RedBlackTree", "Uniform", uniform, run, file);
            run_workload<SkipList<bench::Key>>("SkipList", "Uniform", uniform, run, file);

            run_workload<RedBlackTree<bench::Key>>("RedBlackTree", "Sorted", sorted, run, file);
            run_workload<SkipList<bench::Key>>("SkipList", "Sorted", sorted, run, file);

            run_workload<RedBlackTree<bench::Key>>("RedBlackTree", "Skewed", skewed, run, file);
            run_workload<SkipList<bench::Key>>("SkipList", "Skewed", skewed, run, file);

            if (n <= real_keys.size()) {
                std::vector<std::string> real_subset(real_keys.begin(), real_keys.begin() + n);
                run_workload<RedBlackTree<std::string>>("RedBlackTree", "Real", real_subset, run, file);
                run_workload<SkipList<std::string>>("SkipList", "Real", real_subset, run, file);

            } else {
                std::cout << "  Skipping Real dataset for N=" << n << " (only " << real_keys.size() << " words available)\n";
            }
        }
    }

    file.close();
    std::cout << "Benchmarking complete!\n";
    return 0;
}