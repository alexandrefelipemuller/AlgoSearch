#include "problems/ArgmaxIndexProblem.hpp"

#include <algorithm>

namespace algosearch {

int ArgmaxIndexProblem::answer(const std::vector<int>& values) const {
    int best = 0;
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] > values[static_cast<size_t>(best)]) best = static_cast<int>(i);
    }
    return best;
}

std::vector<std::vector<int>> ArgmaxIndexProblem::constructedArrays(int n, Random& rng) const {
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
