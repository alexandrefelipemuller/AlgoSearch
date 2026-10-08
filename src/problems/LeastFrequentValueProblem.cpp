#include "problems/LeastFrequentValueProblem.hpp"

#include <algorithm>

namespace algosearch {

int LeastFrequentValueProblem::answer(const std::vector<int>& values) const {
    int best = 0, bestCount = 0;
    for (int x : values) {
        int c = static_cast<int>(std::count(values.begin(), values.end(), x));
        if (bestCount == 0 || c < bestCount || (c == bestCount && x < best)) best = x, bestCount = c;
    }
    return best;
}

std::vector<std::vector<int>> LeastFrequentValueProblem::constructedArrays(int n, Random& rng) const {
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 2));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
