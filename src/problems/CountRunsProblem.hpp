#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 52 post-freeze problem `count_runs` (written after the step axes were
// frozen): output[0] = the number of maximal runs of equal adjacent values.
class CountRunsProblem : public ScalarFromArrayProblem {
public:
    explicit CountRunsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
