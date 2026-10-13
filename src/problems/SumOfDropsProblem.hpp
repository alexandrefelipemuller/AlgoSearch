#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 111 problem `sum_of_drops` (written before the transform piece ran, see FINDINGS.md):
// output[0] = the sum of max(0, arr[i-1] - arr[i]) over adjacent pairs (only the drops count) (0 for an empty array).
class SumOfDropsProblem : public ScalarFromArrayProblem {
public:
    explicit SumOfDropsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
