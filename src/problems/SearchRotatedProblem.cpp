#include "problems/SearchRotatedProblem.hpp"

#include <algorithm>

namespace algosearch {

int SearchRotatedProblem::answer(const std::vector<int>& values) const {
    for (size_t i = 1; i < values.size(); ++i) {
        if (values[i] == values[0]) return static_cast<int>(i);
    }
    return -1;
}

std::vector<std::vector<int>> SearchRotatedProblem::constructedArrays(int n, Random& rng) const {
    // Key, then a rotated sorted array of distinct values; key present or absent.
    std::vector<std::vector<int>> out;
    if (n < 2) return out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> hay(static_cast<size_t>(n - 1));
        for (size_t k = 0; k < hay.size(); ++k) hay[k] = 2 * static_cast<int>(k) + 1;
        std::rotate(hay.begin(), hay.begin() + rng.uniformInt(0, n - 2), hay.end());
        int key = c % 2 == 0 ? hay[static_cast<size_t>(rng.uniformInt(0, n - 2))] : 2 * rng.uniformInt(0, n - 1);
        std::vector<int> v{key};
        v.insert(v.end(), hay.begin(), hay.end());
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
