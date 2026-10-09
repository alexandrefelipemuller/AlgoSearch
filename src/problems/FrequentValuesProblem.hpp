#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 87 post-freeze problem `frequent_values` (written after the distinct-count freeze, see FINDINGS.md):
// output[0] = the number of distinct values that occur at least three times.
class FrequentValuesProblem : public ScalarFromArrayProblem {
public:
    explicit FrequentValuesProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
