#include "problems/LongestRunStartProblem.hpp"

#include <algorithm>

namespace algosearch {

int LongestRunStartProblem::answer(const std::vector<int>& values) const {
    int start = 0, bestLen = 0, len = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        len = (i > 0 && values[i] == values[i - 1]) ? len + 1 : 1;
        if (len > bestLen) bestLen = len, start = static_cast<int>(i) - len + 1;
    }
    return start;
}

std::vector<std::vector<int>> LongestRunStartProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so values repeat and runs form.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 3));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
