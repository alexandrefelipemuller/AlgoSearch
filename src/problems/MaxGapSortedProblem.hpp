#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `max_gap_sorted` (written after the library freeze, see FINDINGS.md):
// output[0] = the maximum adjacent difference after sorting (0 when n < 2), LeetCode's "Maximum Gap". Type 1 (sort, then an adjacent pass).
class MaxGapSortedProblem : public ScalarFromArrayProblem {
public:
    explicit MaxGapSortedProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
