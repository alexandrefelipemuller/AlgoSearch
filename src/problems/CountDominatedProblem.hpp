#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 71 post-freeze problem `count_dominated` (written after the nested-aggregate freeze, see FINDINGS.md):
// output[0] = the number of elements with some strictly greater element to their right. Classic (right-to-left running maximum).
class CountDominatedProblem : public ScalarFromArrayProblem {
public:
    explicit CountDominatedProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
