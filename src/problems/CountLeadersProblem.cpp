#include "problems/CountLeadersProblem.hpp"

#include <algorithm>

namespace algosearch {

int CountLeadersProblem::answer(const std::vector<int>& values) const {
    int count = 0, best = 0;
    for (size_t k = values.size(); k-- > 0;) {
        if (k + 1 == values.size() || values[k] > best) {
            ++count;
            best = values[k];
        }
    }
    return count;
}

std::vector<std::vector<int>> CountLeadersProblem::constructedArrays(int n, Random& rng) const {
    // Non-increasing arrays (answer at its extreme) and random wide arrays.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        if (c % 2 == 0) std::sort(v.rbegin(), v.rend());
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
