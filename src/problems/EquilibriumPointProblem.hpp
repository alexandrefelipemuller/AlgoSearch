#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 114 external held-out problem `equilibrium_point` (Tutorialspoint "Easy" array list, see FINDINGS.md):
// output[0] = the first index i with sum(a[0..i-1]) == sum(a[i+1..n-1]), or -1.
class EquilibriumPointProblem : public ScalarFromArrayProblem {
public:
    explicit EquilibriumPointProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
