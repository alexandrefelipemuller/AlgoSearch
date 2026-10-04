#include "problems/LongestIncreasingRunProblem.hpp"

#include <algorithm>

namespace algosearch {

int LongestIncreasingRunProblem::answer(const std::vector<int>& values) const {
    int best = values.empty() ? 0 : 1, run = 1;
    for (size_t i = 1; i < values.size(); ++i) {
        run = values[i - 1] < values[i] ? run + 1 : 1;
        best = std::max(best, run);
    }
    return best;
}

std::vector<std::vector<int>> LongestIncreasingRunProblem::constructedArrays(int n, Random& rng) const {
    // Arrays built from strictly increasing runs of random lengths.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v;
        while (static_cast<int>(v.size()) < n) {
            int len = rng.uniformInt(1, std::max(1, n / 2 + 1));
            int x = rng.uniformInt(-n, n);
            for (int k = 0; k < len && static_cast<int>(v.size()) < n; ++k) v.push_back(x += rng.uniformInt(1, 3));
        }
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
