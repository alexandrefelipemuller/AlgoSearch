#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 71 post-freeze problem `count_unique` (written after the nested-aggregate freeze, see FINDINGS.md):
// output[0] = the number of elements whose value occurs exactly once. Classic (count per element).
class CountUniqueProblem : public ScalarFromArrayProblem {
public:
    explicit CountUniqueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
