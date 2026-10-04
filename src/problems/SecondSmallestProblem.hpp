#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `second_smallest` (written after the library freeze, see FINDINGS.md):
// output[0] = sorted(arr)[1] (duplicates count; sorted(arr)[0] when n = 1). Type 1 (sort, then read one position).
class SecondSmallestProblem : public ScalarFromArrayProblem {
public:
    explicit SecondSmallestProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
