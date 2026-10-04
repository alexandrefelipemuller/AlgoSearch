#include "problems/CountDistinctProblem.hpp"

#include <algorithm>

namespace algosearch {

int CountDistinctProblem::answer(const std::vector<int>& values) const {
    std::vector<int> s = values;
    std::sort(s.begin(), s.end());
    return static_cast<int>(std::unique(s.begin(), s.end()) - s.begin());
}

std::vector<std::vector<int>> CountDistinctProblem::constructedArrays(int n, Random& rng) const {
    // Few distinct values, many repeats.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        int k = rng.uniformInt(1, std::max(1, n / 2));
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, k) * 3 - 2;
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
