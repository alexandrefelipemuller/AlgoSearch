#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 52 post-freeze problem `count_ascending_runs` (written after the step axes were
// frozen): output[0] = the number of maximal strictly increasing contiguous runs.
class CountAscendingRunsProblem : public ScalarFromArrayProblem {
public:
    explicit CountAscendingRunsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
