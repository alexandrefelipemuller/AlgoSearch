#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 82 post-freeze problem `least_frequent_value` (written after the arg-best freeze, see FINDINGS.md):
// output[0] = the least frequent value, the smallest one on ties (0 for an empty array).
class LeastFrequentValueProblem : public ScalarFromArrayProblem {
public:
    explicit LeastFrequentValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
