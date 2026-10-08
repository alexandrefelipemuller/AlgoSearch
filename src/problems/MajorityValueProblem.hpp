#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 82 post-freeze problem `majority_value` (written after the arg-best freeze, see FINDINGS.md):
// output[0] = the value occurring more than n/2 times, or -1 when there is none.
class MajorityValueProblem : public ScalarFromArrayProblem {
public:
    explicit MajorityValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
