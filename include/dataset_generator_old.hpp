#ifndef DATASET_GENERATOR_HPP
#define DATASET_GENERATOR_HPP

#include <vector>
#include <random>
#include <numeric>
#include <algorithm>
#include <string>
#include <fstream>
#include <stdexcept>

namespace ds {
namespace bench {

class DatasetGenerator {
public:
    static std::vector<int> generate_uniform(size_t n, int min_val = 0, int max_val = 1000000, unsigned seed = 42) {
        std::vector<int> data(n);
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(min_val, max_val);
        for (size_t i = 0; i < n; ++i) data[i] = dist(rng);
        return data;
    }

    static std::vector<int> generate_sorted(size_t n, int start = 0) {
        std::vector<int> data(n);
        std::iota(data.begin(), data.end(), start);
        return data;
    }

    static std::vector<int> generate_reverse_sorted(size_t n, int start = 0) {
        std::vector<int> data = generate_sorted(n, start);
        std::reverse(data.begin(), data.end());
        return data;
    }

    // Approximates skewed/Zipfian distribution using a geometric distribution
    static std::vector<int> generate_skewed(size_t n, double p = 0.1, unsigned seed = 42) {
        std::vector<int> data(n);
        std::mt19937 rng(seed);
        std::geometric_distribution<int> dist(p);
        for (size_t i = 0; i < n; ++i) data[i] = dist(rng);
        return data;
    }

    static std::vector<std::string> load_words_from_file(const std::string& filepath, size_t max_words = 0) {
        std::vector<std::string> words;
        std::ifstream file(filepath);
        if (!file.is_open()) throw std::runtime_error("Could not open file: " + filepath);
        
        std::string word;
        while (file >> word) {
            words.push_back(word);
            if (max_words > 0 && words.size() >= max_words) break;
        }
        return words;
    }
};

} // namespace bench
} // namespace ds

#endif // DATASET_GENERATOR_HPP