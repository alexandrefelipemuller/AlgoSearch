#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 71 post-freeze problem `max_frequency` (written after the nested-aggregate freeze, see FINDINGS.md):
// output[0] = the number of occurrences of the most frequent value (0 for an empty array). Classic (count per element, or sort and scan).
class MaxFrequencyProblem : public ScalarFromArrayProblem {
public:
    explicit MaxFrequencyProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
