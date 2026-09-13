#include "problems/ArrayInstances.hpp"

#include <algorithm>
#include <numeric>

#include "core/Random.hpp"

namespace algosearch {

std::vector<std::vector<int>> generateBaseArrays(int n, int exhaustiveMaxN, int randomCount, uint64_t seed) {
    std::vector<std::vector<int>> arrays;
    if (n <= 0) return arrays;

    if (n <= exhaustiveMaxN) {
        std::vector<int> perm(static_cast<size_t>(n));
        std::iota(perm.begin(), perm.end(), 0);
        do {
            arrays.push_back(perm);
        } while (std::next_permutation(perm.begin(), perm.end()));
    } else {
        Random rng(seed + static_cast<uint64_t>(n) * 1000003ULL);
        for (int i = 0; i < randomCount; ++i) {
            std::vector<int> values(static_cast<size_t>(n));
            for (int j = 0; j < n; ++j) values[static_cast<size_t>(j)] = rng.uniformInt(0, n * 4);
            arrays.push_back(std::move(values));
        }
    }
    return arrays;
}

std::vector<std::vector<int>> generateStructuralArrays(int n, uint64_t seed, uint64_t seedOffset, int wideRangeSamples) {
    std::vector<std::vector<int>> arrays;
    if (n <= 0) return arrays;

    arrays.push_back(std::vector<int>(static_cast<size_t>(n), n));  // all-equal
    {
        std::vector<int> dup(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) dup[static_cast<size_t>(i)] = i / 2;  // heavy duplicates
        arrays.push_back(std::move(dup));
    }
    {
        std::vector<int> neg(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) neg[static_cast<size_t>(i)] = (i % 2 == 0 ? -1 : 1) * (i + 1);  // negatives
        arrays.push_back(std::move(neg));
    }
    {
        std::vector<int> sorted(static_cast<size_t>(n));
        std::iota(sorted.begin(), sorted.end(), 0);
        arrays.push_back(sorted);  // already sorted
    }
    {
        std::vector<int> reversed(static_cast<size_t>(n));
        std::iota(reversed.begin(), reversed.end(), 0);
        std::reverse(reversed.begin(), reversed.end());
        arrays.push_back(std::move(reversed));  // worst-case reverse order
    }

    Random rng(seed + seedOffset + static_cast<uint64_t>(n) * 7919ULL);
    for (int s = 0; s < wideRangeSamples; ++s) {
        std::vector<int> wide(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) wide[static_cast<size_t>(i)] = rng.uniformInt(-3 * n - 10, 3 * n + 10);
        arrays.push_back(std::move(wide));
    }
    return arrays;
}

}  // namespace algosearch
