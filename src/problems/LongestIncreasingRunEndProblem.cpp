#include "problems/LongestIncreasingRunEndProblem.hpp"

#include <algorithm>

namespace algosearch {

int LongestIncreasingRunEndProblem::answer(const std::vector<int>& values) const {
    int best = 0, bestLen = 0, len = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        len = (i > 0 && values[i] > values[i - 1]) ? len + 1 : 1;
        if (len > bestLen) bestLen = len, best = values[i];
    }
    return best;
}

std::vector<std::vector<int>> LongestIncreasingRunEndProblem::constructedArrays(int n, Random& rng) const {
    // Random wide arrays, so increasing runs vary in length.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
