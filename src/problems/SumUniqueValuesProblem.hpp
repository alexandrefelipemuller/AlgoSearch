#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 112 problem `sum_unique_values` (written before the distinct-aggregate piece ran, see FINDINGS.md):
// output[0] = the sum of the values occurring exactly once (0 for an empty array).
class SumUniqueValuesProblem : public ScalarFromArrayProblem {
public:
    explicit SumUniqueValuesProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
