#include "problems/CountDominatedProblem.hpp"

#include <algorithm>
#include <cstdlib>

namespace algosearch {

int CountDominatedProblem::answer(const std::vector<int>& values) const {
    int count = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = i + 1; j < values.size(); ++j) {
            if (values[j] > values[i]) {
                count++;
                break;
            }
        }
    }
    return count;
}

std::vector<std::vector<int>> CountDominatedProblem::constructedArrays(int n, Random& rng) const {
    // Non-increasing arrays (answer 0) and random arrays.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(-n, 2 * n);
        if (c % 2 == 0) std::sort(v.rbegin(), v.rend());
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
