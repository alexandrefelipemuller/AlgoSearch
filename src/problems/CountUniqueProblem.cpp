#include "problems/CountUniqueProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int CountUniqueProblem::answer(const std::vector<int>& values) const {
    int count = 0;
    for (int x : values) count += std::count(values.begin(), values.end(), x) == 1 ? 1 : 0;
    return count;
}

std::vector<std::vector<int>> CountUniqueProblem::constructedArrays(int n, Random& rng) const {
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
