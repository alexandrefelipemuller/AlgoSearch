#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 80 post-freeze problem `sum_of_rises` (written after the orthogonal-modifier freeze, see FINDINGS.md):
// output[0] = the sum of max(0, arr[i] - arr[i-1]) over adjacent pairs (only the rises count).
class SumOfRisesProblem : public ScalarFromArrayProblem {
public:
    explicit SumOfRisesProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
