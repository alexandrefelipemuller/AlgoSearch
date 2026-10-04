#include "problems/LongestDecreasingSubsequenceProblem.hpp"

#include <algorithm>

namespace algosearch {

int LongestDecreasingSubsequenceProblem::answer(const std::vector<int>& values) const {
    std::vector<int> dp(values.size(), 1);
    int best = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (values[j] > values[i]) dp[i] = std::max(dp[i], dp[j] + 1);
        }
        best = std::max(best, dp[i]);
    }
    return best;
}

std::vector<std::vector<int>> LongestDecreasingSubsequenceProblem::constructedArrays(int n, Random& rng) const {
    // Mixed signs and spread values, plus all-positive arrays.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        bool allPositive = c % 3 == 0;
        for (int& x : v) x = allPositive ? rng.uniformInt(1, 2 * n) : rng.uniformInt(-n, n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
