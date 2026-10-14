#include "problems/FrequentModeOrNoneProblem.hpp"

#include <algorithm>

namespace algosearch {

int FrequentModeOrNoneProblem::answer(const std::vector<int>& values) const {
    int best = -1, bestCount = 1;
    for (int x : values) {
        int count = static_cast<int>(std::count(values.begin(), values.end(), x));
        if (count > bestCount) bestCount = count, best = x;
    }
    return best;
}

std::vector<std::vector<int>> FrequentModeOrNoneProblem::constructedArrays(int n, Random& rng) const {
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
