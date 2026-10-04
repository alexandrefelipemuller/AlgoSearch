#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 52 post-freeze problem `max_rise` (written after the step axes were
// frozen): output[0] = the largest adjacent step arr[i] - arr[i-1] (negative if the array only falls; 0 when n < 2).
class MaxRiseProblem : public ScalarFromArrayProblem {
public:
    explicit MaxRiseProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
