#include "problems/SecondSmallestProblem.hpp"

#include <algorithm>

namespace algosearch {

int SecondSmallestProblem::answer(const std::vector<int>& values) const {
    std::vector<int> s = values;
    std::sort(s.begin(), s.end());
    if (s.empty()) return 0;
    return s.size() >= 2 ? s[1] : s[0];
}

std::vector<std::vector<int>> SecondSmallestProblem::constructedArrays(int n, Random& rng) const {
    // Spread values, partly negative, with repeats.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
