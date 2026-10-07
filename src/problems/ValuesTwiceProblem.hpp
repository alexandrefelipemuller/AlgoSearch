#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 75 post-freeze problem `values_twice` (written after the frequency-table freeze, see FINDINGS.md):
// output[0] = the number of distinct values that occur exactly twice. Classic (count per element, then per value).
class ValuesTwiceProblem : public ScalarFromArrayProblem {
public:
    explicit ValuesTwiceProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
