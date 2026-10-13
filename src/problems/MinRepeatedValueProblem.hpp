#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 112d problem `min_repeated_value` (written before the max aggregate ran, see FINDINGS.md):
// output[0] = the smallest value occurring at least twice, or -1 when none repeats.
class MinRepeatedValueProblem : public ScalarFromArrayProblem {
public:
    explicit MinRepeatedValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
