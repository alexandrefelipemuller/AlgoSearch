#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 113 problem `longest_increasing_run_end` (written before the arg-sentinel piece ran, see FINDINGS.md):
// output[0] = the last value of the first longest strictly increasing run (0 for an empty array).
class LongestIncreasingRunEndProblem : public ScalarFromArrayProblem {
public:
    explicit LongestIncreasingRunEndProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
