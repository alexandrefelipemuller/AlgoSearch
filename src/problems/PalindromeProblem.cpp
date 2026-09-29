#include "problems/PalindromeProblem.hpp"

#include <algorithm>

namespace algosearch {

int PalindromeProblem::answer(const std::vector<int>& values) const {
    int n = static_cast<int>(values.size());
    for (int i = 0; i < n / 2; ++i) {
        if (values[static_cast<size_t>(i)] != values[static_cast<size_t>(n - 1 - i)]) return 0;
    }
    return 1;
}

std::vector<std::vector<int>> PalindromeProblem::constructedArrays(int n, Random& rng) const {
    // Mirrored halves: palindromes are rare among random arrays.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 20; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int i = 0; i < (n + 1) / 2; ++i) v[static_cast<size_t>(i)] = v[static_cast<size_t>(n - 1 - i)] = rng.uniformInt(0, n);
        out.push_back(v);
        if (n >= 2) {  // near miss: one mismatched pair
            int i = rng.uniformInt(0, n / 2 - 1);
            v[static_cast<size_t>(i)] += 1;
            out.push_back(v);
        }
    }
    return out;
}

}  // namespace algosearch
