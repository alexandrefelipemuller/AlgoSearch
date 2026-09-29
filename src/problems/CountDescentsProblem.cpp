#include "problems/CountDescentsProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int CountDescentsProblem::answer(const std::vector<int>& values) const {
    int count = 0;
    for (size_t i = 1; i < values.size(); ++i) count += values[i - 1] > values[i] ? 1 : 0;
    return count;
}

std::vector<std::vector<int>> CountDescentsProblem::constructedArrays(int n, Random& rng) const {
    // Sorted arrays with a few planted descents, spreading the count.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, n);
        std::sort(v.begin(), v.end());
        int swaps = n >= 2 ? rng.uniformInt(0, n - 1) : 0;
        for (int s = 0; s < swaps; ++s) {
            int i = rng.uniformInt(0, n - 2);
            std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(i + 1)]);
        }
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
