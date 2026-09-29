#include "problems/HasAdjacentEqualProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int HasAdjacentEqualProblem::answer(const std::vector<int>& values) const {
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] == values[i]) return 1;
    }
    return 0;
}

std::vector<std::vector<int>> HasAdjacentEqualProblem::constructedArrays(int n, Random& rng) const {
    // Arrays with no adjacent repeat but non-adjacent repeats (label 0,
    // separating this from has_duplicate), and ones with one planted repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = i % 2 == 0 ? rng.uniformInt(0, 1) : 5 + rng.uniformInt(0, 1);
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
