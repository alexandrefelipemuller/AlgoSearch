#include "problems/FirstGreaterThanFirstProblem.hpp"

#include <algorithm>

namespace algosearch {

int FirstGreaterThanFirstProblem::answer(const std::vector<int>& values) const {
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] > values[0]) return static_cast<int>(i);
    }
    return -1;
}

std::vector<std::vector<int>> FirstGreaterThanFirstProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so the key recurs.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 2));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
