#include "problems/TotalVariationProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int TotalVariationProblem::answer(const std::vector<int>& values) const {
    int total = 0;
    for (size_t i = 1; i < values.size(); ++i) total += std::abs(values[i] - values[i - 1]);
    return total;
}

std::vector<std::vector<int>> TotalVariationProblem::constructedArrays(int n, Random& rng) const {
    // Random wide arrays with mixed directions.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
