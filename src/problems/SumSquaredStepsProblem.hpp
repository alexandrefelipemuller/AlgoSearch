#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 111 problem `sum_squared_steps` (written before the transform piece ran, see FINDINGS.md):
// output[0] = the sum of (arr[i] - arr[i-1])^2 over adjacent pairs (0 for an empty array).
class SumSquaredStepsProblem : public ScalarFromArrayProblem {
public:
    explicit SumSquaredStepsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
