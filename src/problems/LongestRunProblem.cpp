#include "problems/LongestRunProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int LongestRunProblem::answer(const std::vector<int>& values) const {
    int best = values.empty() ? 0 : 1, run = 1;
    for (size_t i = 1; i < values.size(); ++i) {
        run = values[i] == values[i - 1] ? run + 1 : 1;
        best = std::max(best, run);
    }
    return best;
}

std::vector<std::vector<int>> LongestRunProblem::constructedArrays(int n, Random& rng) const {
    // Arrays built from runs of random lengths.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v;
        int value = 0;
        while (static_cast<int>(v.size()) < n) {
            int len = rng.uniformInt(1, std::max(1, n / 2 + 1));
            for (int k = 0; k < len && static_cast<int>(v.size()) < n; ++k) v.push_back(value);
            value += rng.uniformInt(1, 2);
        }
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
