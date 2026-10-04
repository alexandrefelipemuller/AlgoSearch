#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 55 post-freeze problem `min_subarray_sum` (written after the stateful pieces
// were frozen in commit b30674a): output[0] = the minimum sum of a non-empty contiguous subarray.
class MinSubarraySumProblem : public ScalarFromArrayProblem {
public:
    explicit MinSubarraySumProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
