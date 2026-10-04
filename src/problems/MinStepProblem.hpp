#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 52 post-freeze problem `min_step` (written after the step axes were
// frozen): output[0] = the smallest adjacent step arr[i] - arr[i-1], i.e. the steepest drop as a negative number (0 when n < 2).
class MinStepProblem : public ScalarFromArrayProblem {
public:
    explicit MinStepProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
