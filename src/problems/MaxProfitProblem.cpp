#include "problems/MaxProfitProblem.hpp"

#include <algorithm>

namespace algosearch {

int MaxProfitProblem::answer(const std::vector<int>& values) const {
    int best = 0;
    for (size_t j = 0; j < values.size(); ++j) {
        for (size_t i = 0; i < j; ++i) best = std::max(best, values[j] - values[i]);
    }
    return best;
}

std::vector<std::vector<int>> MaxProfitProblem::constructedArrays(int n, Random& rng) const {
    // Non-increasing arrays (answer at its extreme) and random wide arrays.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        if (c % 2 == 0) std::sort(v.rbegin(), v.rend());
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
