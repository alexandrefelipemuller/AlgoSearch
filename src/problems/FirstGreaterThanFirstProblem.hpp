#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 117 problem `first_greater_than_first` (written before the first-index and prefix-split pieces ran, see FINDINGS.md):
// output[0] = the first index i >= 1 with arr[i] > arr[0], or -1.
class FirstGreaterThanFirstProblem : public ScalarFromArrayProblem {
public:
    explicit FirstGreaterThanFirstProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
