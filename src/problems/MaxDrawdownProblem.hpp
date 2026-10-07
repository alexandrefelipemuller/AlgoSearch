#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 71 post-freeze problem `max_drawdown` (written after the nested-aggregate freeze, see FINDINGS.md):
// output[0] = max(0, max over i < j of arr[i] - arr[j]), the largest drop. Classic (running maximum).
class MaxDrawdownProblem : public ScalarFromArrayProblem {
public:
    explicit MaxDrawdownProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
