#include "problems/MaxSumIncreasingSubsequenceProblem.hpp"

#include <algorithm>

namespace algosearch {

int MaxSumIncreasingSubsequenceProblem::answer(const std::vector<int>& values) const {
    std::vector<int> dp(values.size(), 0);
    int best = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        int prev = 0;
        for (size_t j = 0; j < i; ++j) {
            if (values[j] < values[i]) prev = std::max(prev, dp[j]);
        }
        dp[i] = prev + values[i];
        best = std::max(best, dp[i]);
    }
    return best;
}

std::vector<std::vector<int>> MaxSumIncreasingSubsequenceProblem::constructedArrays(int n, Random& rng) const {
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
