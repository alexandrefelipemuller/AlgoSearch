#include "problems/RepeatedValuesProblem.hpp"

#include <algorithm>

namespace algosearch {

int RepeatedValuesProblem::answer(const std::vector<int>& values) const {
    int count = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        bool first = std::find(values.begin(), values.begin() + static_cast<long>(i), values[i]) == values.begin() + static_cast<long>(i);
        if (first && std::count(values.begin(), values.end(), values[i]) >= 2) count++;
    }
    return count;
}

std::vector<std::vector<int>> RepeatedValuesProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so values repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 2));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
