#include "problems/StrictlyIncreasingProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int StrictlyIncreasingProblem::answer(const std::vector<int>& values) const {
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] >= values[i]) return 0;
    }
    return 1;
}

std::vector<std::vector<int>> StrictlyIncreasingProblem::constructedArrays(int n, Random& rng) const {
    // Strictly increasing arrays (label 1) and near misses: one equal pair.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        int x = rng.uniformInt(-2, 2);
        for (int& y : v) { y = x; x += rng.uniformInt(1, 3); }
        out.push_back(v);
        if (n >= 2) {
            int i = rng.uniformInt(1, n - 1);
            v[static_cast<size_t>(i)] = v[static_cast<size_t>(i - 1)];
            out.push_back(v);
        }
    }
    return out;
}

}  // namespace algosearch
