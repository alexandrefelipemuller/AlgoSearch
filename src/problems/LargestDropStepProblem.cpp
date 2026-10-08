#include "problems/LargestDropStepProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int LargestDropStepProblem::answer(const std::vector<int>& values) const {
    int best = 0;
    for (size_t i = 1; i < values.size(); ++i) {
        int drop = values[i - 1] - values[i];
        if (i == 1 || drop > best) best = drop;
    }
    return best;
}

std::vector<std::vector<int>> LargestDropStepProblem::constructedArrays(int n, Random& rng) const {
    // Random wide arrays, half of them sorted (every step rises).
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        if (c % 2 == 0) std::sort(v.begin(), v.end());
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
