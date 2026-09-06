#include "skip_list.hpp"
#include "red_black_tree.hpp"
#include "dataset_generator.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

using namespace ds;
using namespace std::chrono;

// Pass the open file stream instead of a vector
template <typename Structure>
void run_workload(const std::string& struct_name, const std::string& dataset_name, 
                  const std::vector<int>& keys, int run, std::ofstream& file) {
    
    Structure ds;
    size_t n = keys.size();
    
    // 1. Insertion Phase
    ds.reset_comparisons();
    auto start = steady_clock::now();
    for (int key : keys) ds.insert(key);
    auto end = steady_clock::now();
    
    double insert_ms = duration_cast<duration<double, std::milli>>(end - start).count();
    
    int current_height = 0;
    if constexpr (std::is_same_v<Structure, RedBlackTree<int>>) current_height = ds.height();
    else current_height = ds.current_level();

    // Write directly to file and flush to save immediately
    file << struct_name << "," << dataset_name << "," << n << "," << run << ","
         << "Insert," << insert_ms << "," << ds.get_comparisons() << "," 
         << ds.memory_footprint() << "," << current_height << "\n";
    file.flush();

    // 2. Search Phase 
    std::vector<int> search_keys = keys;
    std::mt19937 rng(run);
    std::shuffle(search_keys.begin(), search_keys.end(), rng);
    for(size_t i = 0; i < search_keys.size() / 10; ++i) search_keys[i] = -search_keys[i]; 

    ds.reset_comparisons();
    start = steady_clock::now();
    for (int key : search_keys) ds.search(key);
    end = steady_clock::now();
    
    double search_ms = duration_cast<duration<double, std::milli>>(end - start).count();
    
    file << struct_name << "," << dataset_name << "," << n << "," << run << ","
         << "Search," << search_ms << "," << ds.get_comparisons() << "," 
         << ds.memory_footprint() << "," << current_height << "\n";
    file.flush();

    // 3. Deletion Phase
    std::vector<int> delete_keys = keys;
    std::shuffle(delete_keys.begin(), delete_keys.end(), rng);
    
    ds.reset_comparisons();
    start = steady_clock::now();
    for (int key : delete_keys) ds.remove(key);
    end = steady_clock::now();
    
    double delete_ms = duration_cast<duration<double, std::milli>>(end - start).count();
    
    file << struct_name << "," << dataset_name << "," << n << "," << run << ","
         << "Delete," << delete_ms << "," << ds.get_comparisons() << "," 
         << ds.memory_footprint() << ",0\n";
    file.flush();
}

int main() {
    // For initial testing, you might want to use a smaller N and fewer runs 
    // to ensure everything works before doing the full sweep.
    std::vector<size_t> N_values = {1000, 10000, 100000}; 
    int num_runs = 3; 
    
    std::string filename = "benchmark_results.csv";
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not create file " << filename << "\n";
        return 1;
    }

    std::cout << "Starting benchmarks. Writing to " << filename << "...\n";
    file << "Structure,Dataset,N,Run,Operation,Time_ms,Comparisons,Memory_bytes,Height\n";
    file.flush();

    for (size_t n : N_values) {
        std::cout << "Testing N = " << n << "\n";
        for (int run = 1; run <= num_runs; ++run) {
            auto uniform = bench::DatasetGenerator::generate_uniform(n, 0, n * 10, run);
            
            run_workload<RedBlackTree<int>>("RedBlackTree", "Uniform", uniform, run, file);
            run_workload<SkipList<int>>("SkipList", "Uniform", uniform, run, file);
            
            // Note: I temporarily removed Sorted and Skewed to speed up your initial test.
            // You can add them back once you confirm the file generates correctly.
        }
    }

    file.close();
    std::cout << "Benchmarking complete!\n";
    return 0;
}