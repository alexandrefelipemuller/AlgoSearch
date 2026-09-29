#include "problems/IsSortedProblem.hpp"

#include <algorithm>

namespace algosearch {

int IsSortedProblem::answer(const std::vector<int>& values) const {
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] > values[i]) return 0;
    }
    return 1;
}

std::vector<std::vector<int>> IsSortedProblem::constructedArrays(int n, Random& rng) const {
    // Sorted arrays (label 1) and sorted arrays with one adjacent swap.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, n);
        std::sort(v.begin(), v.end());
        out.push_back(v);
        if (n >= 2) {
            int i = rng.uniformInt(0, n - 2);
            if (v[static_cast<size_t>(i)] != v[static_cast<size_t>(i + 1)]) {
                std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(i + 1)]);
                out.push_back(v);
            }
        }
    }
    return out;
}

}  // namespace algosearch
