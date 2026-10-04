#include "problems/CeilingWaterProblem.hpp"

#include <algorithm>

namespace algosearch {

int CeilingWaterProblem::answer(const std::vector<int>& values) const {
    int total = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        int left = *std::min_element(values.begin(), values.begin() + static_cast<long>(i) + 1);
        int right = *std::min_element(values.begin() + static_cast<long>(i), values.end());
        total += values[i] - std::max(left, right);
    }
    return total;
}

std::vector<std::vector<int>> CeilingWaterProblem::constructedArrays(int n, Random& rng) const {
    // Heights with peaks hanging between lower ceiling points.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int k = 0; k < n; ++k) v[static_cast<size_t>(k)] = k % 3 == 0 ? rng.uniformInt(0, n / 2) : rng.uniformInt(n / 2, n + 2);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
