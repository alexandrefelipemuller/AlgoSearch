#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 87 post-freeze problem `has_double_value` (written after the distinct-count freeze, see FINDINGS.md):
// output[0] = 1 if some value occurs exactly twice, else 0.
class HasDoubleValueProblem : public ScalarFromArrayProblem {
public:
    explicit HasDoubleValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
