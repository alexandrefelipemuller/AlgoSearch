#include "problems/HasDuplicateProblem.hpp"

#include <algorithm>

namespace algosearch {

int HasDuplicateProblem::answer(const std::vector<int>& values) const {
    int n = static_cast<int>(values.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (values[static_cast<size_t>(i)] == values[static_cast<size_t>(j)]) return 1;
        }
    }
    return 0;
}

std::vector<std::vector<int>> HasDuplicateProblem::constructedArrays(int n, Random& rng) const {
    // Distinct arrays (label 0) and arrays with exactly one planted repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 10; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = i * 2 + rng.uniformInt(0, 1);
        for (int i = n - 1; i > 0; --i) std::swap(v[static_cast<size_t>(i)], v[static_cast<size_t>(rng.uniformInt(0, i))]);
        out.push_back(v);
        if (n >= 2) {
            int a = rng.uniformInt(0, n - 1), b = rng.uniformInt(0, n - 2);
            if (b >= a) ++b;
            v[static_cast<size_t>(b)] = v[static_cast<size_t>(a)];
            out.push_back(v);
        }
    }
    return out;
}

}  // namespace algosearch
