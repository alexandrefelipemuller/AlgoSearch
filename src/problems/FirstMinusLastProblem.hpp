#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 80 post-freeze problem `first_minus_last` (written after the orthogonal-modifier freeze, see FINDINGS.md):
// output[0] = arr[0] - arr[n-1] (0 for an empty array). A telescoping sum of reversed steps.
class FirstMinusLastProblem : public ScalarFromArrayProblem {
public:
    explicit FirstMinusLastProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
