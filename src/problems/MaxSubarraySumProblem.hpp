#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `max_subarray_sum` (written after the library freeze, see FINDINGS.md):
// output[0] = the maximum sum of a non-empty contiguous subarray (Kadane). Type 2 (running state that resets).
class MaxSubarraySumProblem : public ScalarFromArrayProblem {
public:
    explicit MaxSubarraySumProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
