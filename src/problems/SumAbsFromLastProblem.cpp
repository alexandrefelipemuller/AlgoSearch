#include "problems/SumAbsFromLastProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int SumAbsFromLastProblem::answer(const std::vector<int>& values) const {
    if (values.empty()) return 0;
    int total = 0;
    for (int x : values) total += std::abs(x - values.back());
    return total;
}

std::vector<std::vector<int>> SumAbsFromLastProblem::constructedArrays(int n, Random& rng) const {
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
