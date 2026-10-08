#include "problems/MajorityValueProblem.hpp"

#include <algorithm>

namespace algosearch {

int MajorityValueProblem::answer(const std::vector<int>& values) const {
    for (int x : values) {
        if (2 * std::count(values.begin(), values.end(), x) > static_cast<long>(values.size())) return x;
    }
    return -1;
}

std::vector<std::vector<int>> MajorityValueProblem::constructedArrays(int n, Random& rng) const {
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 3));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
