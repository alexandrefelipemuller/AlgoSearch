#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 112 problem `triple_frequency_values` (written before the distinct-aggregate piece ran, see FINDINGS.md):
// output[0] = the number of distinct values whose frequency is a multiple of three (0 for an empty array).
class TripleFrequencyValuesProblem : public ScalarFromArrayProblem {
public:
    explicit TripleFrequencyValuesProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
