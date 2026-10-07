#include "problems/MinFrequencyProblem.hpp"

#include <algorithm>

namespace algosearch {

int MinFrequencyProblem::answer(const std::vector<int>& values) const {
    int best = 0;
    for (int x : values) {
        int c = static_cast<int>(std::count(values.begin(), values.end(), x));
        if (best == 0 || c < best) best = c;
    }
    return best;
}

std::vector<std::vector<int>> MinFrequencyProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so values repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 2));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
