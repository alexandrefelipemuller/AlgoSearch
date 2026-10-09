#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 87 post-freeze problem `odd_frequency_values` (written after the distinct-count freeze, see FINDINGS.md):
// output[0] = the number of distinct values whose frequency is odd.
class OddFrequencyValuesProblem : public ScalarFromArrayProblem {
public:
    explicit OddFrequencyValuesProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
