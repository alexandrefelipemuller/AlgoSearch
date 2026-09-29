#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 40 post-freeze problem `max_adjacent_gap` (written after combinator v2 was
// frozen in commit 77c4f77): output[0] = the maximum of |arr[i] - arr[i-1]| (0 when there is no pair).
class MaxAdjacentGapProblem : public ScalarFromArrayProblem {
public:
    explicit MaxAdjacentGapProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
