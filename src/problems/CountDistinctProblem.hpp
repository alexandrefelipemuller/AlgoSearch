#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `count_distinct` (written after the library freeze, see FINDINGS.md):
// output[0] = the number of distinct values. Type 1 (sort, then count adjacent changes).
class CountDistinctProblem : public ScalarFromArrayProblem {
public:
    explicit CountDistinctProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
