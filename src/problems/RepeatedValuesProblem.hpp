#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 86 post-freeze problem `repeated_values` (written after the first-occurrence freeze, see FINDINGS.md):
// output[0] = the number of distinct values that occur at least twice.
class RepeatedValuesProblem : public ScalarFromArrayProblem {
public:
    explicit RepeatedValuesProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
