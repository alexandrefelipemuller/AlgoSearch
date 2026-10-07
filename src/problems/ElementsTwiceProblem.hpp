#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 75 post-freeze problem `elements_twice` (written after the frequency-table freeze, see FINDINGS.md):
// output[0] = the number of elements whose value occurs exactly twice. Classic (count per element).
class ElementsTwiceProblem : public ScalarFromArrayProblem {
public:
    explicit ElementsTwiceProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
