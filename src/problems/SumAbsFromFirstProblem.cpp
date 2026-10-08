#include "problems/SumAbsFromFirstProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int SumAbsFromFirstProblem::answer(const std::vector<int>& values) const {
    int total = 0;
    for (int x : values) total += std::abs(x - values.front());
    return total;
}

std::vector<std::vector<int>> SumAbsFromFirstProblem::constructedArrays(int n, Random& rng) const {
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
