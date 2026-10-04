#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 55 post-freeze problem `longest_decreasing_subsequence` (written after the stateful pieces
// were frozen in commit b30674a): output[0] = the length of the longest strictly decreasing (not necessarily contiguous) subsequence.
class LongestDecreasingSubsequenceProblem : public ScalarFromArrayProblem {
public:
    explicit LongestDecreasingSubsequenceProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
