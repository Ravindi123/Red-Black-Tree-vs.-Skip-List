#include "../skip_list.hpp"
#include "../red_black_tree.hpp"
#include "../dataset_generator.hpp"
#include <iostream>
#include <cassert>

using namespace ds;

void test_instrumentation() {
    std::cout << "Testing Instrumentation...\n";
    
    RedBlackTree<int> rbt;
    rbt.insert(10);
    rbt.insert(20);
    
    // A search for an existing element should trigger comparisons
    rbt.reset_comparisons();
    rbt.search(20);
    assert(rbt.get_comparisons() > 0);
    
    size_t rbt_mem = rbt.memory_footprint();
    assert(rbt_mem > sizeof(RedBlackTree<int>));

    SkipList<int> sl;
    sl.insert(10);
    sl.insert(20);
    
    sl.reset_comparisons();
    sl.search(20);
    assert(sl.get_comparisons() > 0);
    
    size_t sl_mem = sl.memory_footprint();
    assert(sl_mem > sizeof(SkipList<int>));
    
    std::cout << "[PASS] Instrumentation tests passed.\n";
}

void test_data_generators() {
    std::cout << "Testing Data Generators...\n";
    
    auto sorted = bench::DatasetGenerator::generate_sorted(100);
    assert(sorted.size() == 100);
    assert(sorted[0] == 0 && sorted[99] == 99);
    
    auto reversed = bench::DatasetGenerator::generate_reverse_sorted(100);
    assert(reversed[0] == 99 && reversed[99] == 0);

    auto skewed = bench::DatasetGenerator::generate_skewed(100, 0.1);
    assert(skewed.size() == 100);

    try {
        std::string filepath = "../Datasets/google-10000-english-usa.txt"; // Adjust path as necessary
        auto words = bench::DatasetGenerator::load_words_from_file(filepath);
        assert(!words.empty());
        std::cout << "Successfully loaded " << words.size() << " words from file.\n";
    } catch (const std::exception& e) {
        std::cerr << "File load failed (this is expected if the file doesn't exist yet): " << e.what() << "\n";
    }
    
    std::cout << "[PASS] Data generation tests passed.\n";
}

int main() {
    test_instrumentation();
    test_data_generators();
    std::cout << "All Instrumentation and Data Generation tests passed successfully!\n";
    return 0;
}