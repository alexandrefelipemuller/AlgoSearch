#include "problems/MaxGapSortedProblem.hpp"

#include <algorithm>

namespace algosearch {

int MaxGapSortedProblem::answer(const std::vector<int>& values) const {
    std::vector<int> s = values;
    std::sort(s.begin(), s.end());
    int best = 0;
    for (size_t i = 1; i < s.size(); ++i) best = std::max(best, s[i] - s[i - 1]);
    return best;
}

std::vector<std::vector<int>> MaxGapSortedProblem::constructedArrays(int n, Random& rng) const {
    // Distinct, widely spread values, so gaps vary.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v;
        int x = rng.uniformInt(-n, n);
        for (int k = 0; k < n; ++k) {
            v.push_back(x);
            x += rng.uniformInt(1, 2 * n);
        }
        for (int k = n - 1; k > 0; --k) std::swap(v[static_cast<size_t>(k)], v[static_cast<size_t>(rng.uniformInt(0, k))]);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
