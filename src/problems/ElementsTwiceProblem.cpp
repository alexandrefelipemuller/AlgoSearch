#include "problems/ElementsTwiceProblem.hpp"

#include <algorithm>

namespace algosearch {

int ElementsTwiceProblem::answer(const std::vector<int>& values) const {
    int count = 0;
    for (int x : values) count += std::count(values.begin(), values.end(), x) == 2 ? 1 : 0;
    return count;
}

std::vector<std::vector<int>> ElementsTwiceProblem::constructedArrays(int n, Random& rng) const {
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
