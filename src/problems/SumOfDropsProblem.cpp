#include "problems/SumOfDropsProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int SumOfDropsProblem::answer(const std::vector<int>& values) const {
    if (values.empty()) return 0;
    int total = 0;
    for (size_t i = 1; i < values.size(); ++i) total += std::max(0, values[i - 1] - values[i]);
    return total;
}

std::vector<std::vector<int>> SumOfDropsProblem::constructedArrays(int n, Random& rng) const {
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
