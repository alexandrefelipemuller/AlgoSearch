#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 112 problem `max_repeated_value` (written before the distinct-aggregate piece ran, see FINDINGS.md):
// output[0] = the largest value occurring at least twice, or 0 when none repeats (0 for an empty array).
class MaxRepeatedValueProblem : public ScalarFromArrayProblem {
public:
    explicit MaxRepeatedValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
