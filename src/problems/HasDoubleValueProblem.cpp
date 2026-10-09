#include "problems/HasDoubleValueProblem.hpp"

#include <algorithm>

namespace algosearch {

int HasDoubleValueProblem::answer(const std::vector<int>& values) const {
    for (int x : values) {
        if (std::count(values.begin(), values.end(), x) == 2) return 1;
    }
    return 0;
}

std::vector<std::vector<int>> HasDoubleValueProblem::constructedArrays(int n, Random& rng) const {
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
