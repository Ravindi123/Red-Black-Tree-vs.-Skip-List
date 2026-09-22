#ifndef DATASET_GENERATORS_HPP
#define DATASET_GENERATORS_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <random>
#include <algorithm>
#include <unordered_set>
#include <stdexcept>
#include <limits>

namespace bench {

using Key = std::int64_t;

// Floyd's algorithm: sample `n` distinct values from [0, range) uniformly at
// random, in random order, without materializing the whole range. O(n) time
// and space, as long as n << range. This avoids the birthday-paradox collision 
// problem.
inline std::vector<Key> floyd_sample(std::int64_t n, std::int64_t range, unsigned seed) {
    if (n > range) {
        throw std::invalid_argument("floyd_sample: n cannot exceed range");
    }
    std::mt19937_64 rng(seed);
    std::unordered_set<Key> chosen;
    chosen.reserve(static_cast<size_t>(n) * 2);
    std::vector<Key> result;
    result.reserve(static_cast<size_t>(n));

    for (std::int64_t j = range - n; j < range; ++j) {
        std::uniform_int_distribution<std::int64_t> dist(0, j);
        Key t = dist(rng);
        if (chosen.find(t) != chosen.end()) {
            chosen.insert(j);
            result.push_back(j);
        } else {
            chosen.insert(t);
            result.push_back(t);
        }
    }
    std::shuffle(result.begin(), result.end(), rng);
    return result;
}

// Uniform random keys, drawn from a domain 10x the size of n so that "uniform"
// insertions almost never collide with themselves during dataset generation.
inline std::vector<Key> generate_uniform(std::int64_t n, unsigned seed) {
    std::int64_t range = std::max<std::int64_t>(n * 10, 1'000'000);
    return floyd_sample(n, range, seed);
}

// Classic adversarial input for a naive (unbalanced) BST: strictly increasing.
inline std::vector<Key> generate_sorted(std::int64_t n) {
    std::vector<Key> out(static_cast<size_t>(n));
    for (std::int64_t i = 0; i < n; ++i) out[static_cast<size_t>(i)] = i;
    return out;
}

// Duplicate-heavy / skewed: n insert *attempts* drawn (with replacement) from
// a small domain of only n/50 distinct values (min 8). 
inline std::vector<Key> generate_skewed(std::int64_t n, unsigned seed) {
    std::int64_t domain_size = std::max<std::int64_t>(n / 50, 8);
    std::vector<Key> domain = floyd_sample(domain_size, std::max<std::int64_t>(domain_size * 10, 1000), seed);

    std::mt19937_64 rng(seed + 1);
    std::uniform_int_distribution<size_t> pick(0, domain.size() - 1);
    std::vector<Key> out;
    out.reserve(static_cast<size_t>(n));
    for (std::int64_t i = 0; i < n; ++i) out.push_back(domain[pick(rng)]);
    return out;
}

// Real-world, non-synthetic dataset.
inline std::vector<std::string> load_real_world(const std::string& filepath, size_t max_words = 0 ) {
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

} // namespace bench

#endif // DATASET_GENERATORS_HPP
