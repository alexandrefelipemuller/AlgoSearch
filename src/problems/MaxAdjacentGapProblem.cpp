#include "problems/MaxAdjacentGapProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int MaxAdjacentGapProblem::answer(const std::vector<int>& values) const {
    int best = 0;
    for (size_t i = 1; i < values.size(); ++i) best = std::max(best, std::abs(values[i] - values[i - 1]));
    return best;
}

std::vector<std::vector<int>> MaxAdjacentGapProblem::constructedArrays(int n, Random& rng) const {
    // Smooth arrays with one planted jump, so the answer is a single pair.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        int x = rng.uniformInt(0, 3);
        for (int& y : v) { y = x; x += rng.uniformInt(-1, 1); }
        if (n >= 2) v[static_cast<size_t>(rng.uniformInt(1, n - 1))] += rng.chance(0.5) ? 7 : -7;
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
