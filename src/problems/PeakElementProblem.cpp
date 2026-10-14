#include "problems/PeakElementProblem.hpp"

#include <algorithm>

namespace algosearch {

int PeakElementProblem::answer(const std::vector<int>& values) const {
    int n = static_cast<int>(values.size());
    for (int i = 0; i < n; ++i) {
        if ((i == 0 || values[i] >= values[i - 1]) && (i == n - 1 || values[i] >= values[i + 1])) return i;
    }
    return 0;
}

std::vector<std::vector<int>> PeakElementProblem::constructedArrays(int n, Random& rng) const {
    // Random wide arrays, so the first peak is often not at index 0.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
