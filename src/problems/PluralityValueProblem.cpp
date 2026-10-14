#include "problems/PluralityValueProblem.hpp"

#include <algorithm>

namespace algosearch {

int PluralityValueProblem::answer(const std::vector<int>& values) const {
    int best = -1, bestCount = 0;
    bool tied = false;
    for (int x : values) {
        int count = static_cast<int>(std::count(values.begin(), values.end(), x));
        if (count > bestCount) bestCount = count, best = x, tied = false;
        else if (count == bestCount && x != best) tied = true;
    }
    return tied ? -1 : best;
}

std::vector<std::vector<int>> PluralityValueProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so values repeat and runs form.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 3));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
