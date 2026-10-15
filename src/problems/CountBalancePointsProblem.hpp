#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 117 problem `count_balance_points` (written before the first-index and prefix-split pieces ran, see FINDINGS.md):
// output[0] = the number of indices i with sum(a[0..i-1]) == sum(a[i+1..n-1]).
class CountBalancePointsProblem : public ScalarFromArrayProblem {
public:
    explicit CountBalancePointsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
