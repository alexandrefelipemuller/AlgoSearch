#include "problems/MaxRepeatedValueProblem.hpp"

#include <algorithm>

namespace algosearch {

int MaxRepeatedValueProblem::answer(const std::vector<int>& values) const {
    // Values are never negative here, so 0 doubles as "none repeats".
    int total = 0;
    for (int x : values) {
        if (x > total && std::count(values.begin(), values.end(), x) >= 2) total = x;
    }
    return total;
}

std::vector<std::vector<int>> MaxRepeatedValueProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so values repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 3));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
