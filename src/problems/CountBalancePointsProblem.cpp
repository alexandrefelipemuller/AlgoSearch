#include "problems/CountBalancePointsProblem.hpp"

#include <algorithm>

namespace algosearch {

int CountBalancePointsProblem::answer(const std::vector<int>& values) const {
    int total = 0, left = 0, count = 0;
    for (int x : values) total += x;
    for (int x : values) {
        if (left == total - left - x) count++;
        left += x;
    }
    return count;
}

std::vector<std::vector<int>> CountBalancePointsProblem::constructedArrays(int n, Random& rng) const {
    // Arrays with zeros around a balance point, so several splits can balance.
    std::vector<std::vector<int>> out;
    if (n < 1) return out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, 2) == 0 ? 0 : rng.uniformInt(1, n);
        int pivot = rng.uniformInt(0, n - 1);
        int left = 0, right = 0;
        for (int k = 0; k < pivot; ++k) left += v[static_cast<size_t>(k)];
        for (int k = pivot + 1; k < n; ++k) right += v[static_cast<size_t>(k)];
        if (left < right && pivot > 0) v[0] += right - left;
        if (right < left && pivot < n - 1) v[static_cast<size_t>(n - 1)] += left - right;
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
