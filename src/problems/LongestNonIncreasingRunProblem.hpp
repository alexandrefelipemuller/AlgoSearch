#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 55 post-freeze problem `longest_non_increasing_run` (written after the stateful pieces
// were frozen in commit b30674a): output[0] = the length of the longest contiguous run with arr[i-1] >= arr[i] (0 for an empty array).
class LongestNonIncreasingRunProblem : public ScalarFromArrayProblem {
public:
    explicit LongestNonIncreasingRunProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
