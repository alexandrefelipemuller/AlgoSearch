#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 111 problem `sum_excess_over_first` (written before the transform piece ran, see FINDINGS.md):
// output[0] = the sum of max(0, arr[i] - arr[0]) over all i (0 for an empty array).
class SumExcessOverFirstProblem : public ScalarFromArrayProblem {
public:
    explicit SumExcessOverFirstProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
