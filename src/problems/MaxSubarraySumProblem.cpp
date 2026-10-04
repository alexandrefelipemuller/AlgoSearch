#include "problems/MaxSubarraySumProblem.hpp"

#include <algorithm>

namespace algosearch {

int MaxSubarraySumProblem::answer(const std::vector<int>& values) const {
    int best = values.empty() ? 0 : values[0], cur = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        cur = i == 0 || cur < 0 ? values[i] : cur + values[i];
        best = std::max(best, cur);
    }
    return best;
}

std::vector<std::vector<int>> MaxSubarraySumProblem::constructedArrays(int n, Random& rng) const {
    // Mixed signs, plus all-negative arrays (the best subarray is one element).
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        bool allNegative = c % 3 == 0;
        for (int& x : v) x = allNegative ? rng.uniformInt(-2 * n, -1) : rng.uniformInt(-n, n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
