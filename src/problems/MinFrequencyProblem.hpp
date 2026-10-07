#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 75 post-freeze problem `min_frequency` (written after the frequency-table freeze, see FINDINGS.md):
// output[0] = the number of occurrences of the least frequent value (0 for an empty array). Classic (count per element).
class MinFrequencyProblem : public ScalarFromArrayProblem {
public:
    explicit MinFrequencyProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
