#include "problems/MinSubarraySumProblem.hpp"

#include <algorithm>

namespace algosearch {

int MinSubarraySumProblem::answer(const std::vector<int>& values) const {
    int best = values.empty() ? 0 : values[0], cur = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        cur = i == 0 || cur > 0 ? values[i] : cur + values[i];
        best = std::min(best, cur);
    }
    return best;
}

std::vector<std::vector<int>> MinSubarraySumProblem::constructedArrays(int n, Random& rng) const {
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
