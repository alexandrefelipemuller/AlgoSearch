#include "problems/MedianProblem.hpp"

#include <algorithm>

namespace algosearch {

int MedianProblem::answer(const std::vector<int>& values) const {
    std::vector<int> s = values;
    std::sort(s.begin(), s.end());
    return s.empty() ? 0 : s[(s.size() - 1) / 2];
}

std::vector<std::vector<int>> MedianProblem::constructedArrays(int n, Random& rng) const {
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
