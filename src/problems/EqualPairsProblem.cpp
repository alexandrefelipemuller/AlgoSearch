#include "problems/EqualPairsProblem.hpp"

#include <algorithm>

namespace algosearch {

int EqualPairsProblem::answer(const std::vector<int>& values) const {
    int count = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = i + 1; j < values.size(); ++j) count += values[i] == values[j] ? 1 : 0;
    }
    return count;
}

std::vector<std::vector<int>> EqualPairsProblem::constructedArrays(int n, Random& rng) const {
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
