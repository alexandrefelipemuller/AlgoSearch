#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 75 post-freeze problem `mode` (written after the frequency-table freeze, see FINDINGS.md):
// output[0] = the most frequent value, the smallest one on ties (0 for an empty array). Classic (count per element, keep the argmax).
class ModeProblem : public ScalarFromArrayProblem {
public:
    explicit ModeProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
