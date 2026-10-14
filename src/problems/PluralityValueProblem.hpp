#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 113 problem `plurality_value` (written before the arg-sentinel piece ran, see FINDINGS.md):
// output[0] = the most frequent value, or -1 when two values tie for the top frequency (-1 for an empty array).
class PluralityValueProblem : public ScalarFromArrayProblem {
public:
    explicit PluralityValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
