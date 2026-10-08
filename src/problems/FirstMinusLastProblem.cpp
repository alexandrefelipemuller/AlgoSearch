#include "problems/FirstMinusLastProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int FirstMinusLastProblem::answer(const std::vector<int>& values) const {
    return values.empty() ? 0 : values.front() - values.back();
}

std::vector<std::vector<int>> FirstMinusLastProblem::constructedArrays(int n, Random& rng) const {
    // Random wide arrays.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
