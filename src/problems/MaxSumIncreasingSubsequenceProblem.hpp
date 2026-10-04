#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 55 post-freeze problem `max_sum_increasing_subsequence` (written after the stateful pieces
// were frozen in commit b30674a): output[0] = the maximum sum of a strictly increasing subsequence; the empty subsequence counts as 0, so the answer is never negative.
class MaxSumIncreasingSubsequenceProblem : public ScalarFromArrayProblem {
public:
    explicit MaxSumIncreasingSubsequenceProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
