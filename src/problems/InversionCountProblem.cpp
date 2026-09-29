#include "problems/InversionCountProblem.hpp"

#include <algorithm>

namespace algosearch {

int InversionCountProblem::answer(const std::vector<int>& values) const {
    int n = static_cast<int>(values.size());
    int count = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) count += values[static_cast<size_t>(i)] > values[static_cast<size_t>(j)] ? 1 : 0;
    }
    return count;
}

std::vector<std::vector<int>> InversionCountProblem::constructedArrays(int n, Random& rng) const {
    // Permutations spread over the whole 0..n(n-1)/2 range of counts.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 20; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = i;
        for (int i = n - 1; i > 0; --i) std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(rng.uniformInt(0, i))]);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
