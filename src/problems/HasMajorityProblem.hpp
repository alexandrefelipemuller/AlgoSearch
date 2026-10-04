#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `has_majority` (written after the library freeze, see FINDINGS.md):
// output[0] = 1 if some value occurs more than n/2 times, else 0. Classic (Boyer-Moore voting, or count per element).
class HasMajorityProblem : public ScalarFromArrayProblem {
public:
    explicit HasMajorityProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
