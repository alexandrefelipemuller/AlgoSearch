#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `min_gap` (written after the library freeze, see FINDINGS.md):
// output[0] = the minimum difference between two values, i.e. the minimum adjacent difference after sorting (0 when n < 2). Type 1 (sort, then an adjacent pass; no absolute value needed once sorted).
class MinGapProblem : public ScalarFromArrayProblem {
public:
    explicit MinGapProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
