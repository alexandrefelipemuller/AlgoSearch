#include "problems/HasMajorityProblem.hpp"

#include <algorithm>

namespace algosearch {

int HasMajorityProblem::answer(const std::vector<int>& values) const {
    for (int x : values) {
        int c = static_cast<int>(std::count(values.begin(), values.end(), x));
        if (2 * c > static_cast<int>(values.size())) return 1;
    }
    return 0;
}

std::vector<std::vector<int>> HasMajorityProblem::constructedArrays(int n, Random& rng) const {
    // One value planted in more than half the positions (label 1).
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, n);
        int m = rng.uniformInt(0, n);
        int planted = n / 2 + 1;
        for (int k = n - 1; k > 0; --k) std::swap(v[static_cast<size_t>(k)], v[static_cast<size_t>(rng.uniformInt(0, k))]);
        for (int k = 0; k < planted; ++k) v[static_cast<size_t>(k)] = m;
        for (int k = n - 1; k > 0; --k) std::swap(v[static_cast<size_t>(k)], v[static_cast<size_t>(rng.uniformInt(0, k))]);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
