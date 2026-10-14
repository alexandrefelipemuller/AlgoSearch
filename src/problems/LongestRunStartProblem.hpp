#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 113 problem `longest_run_start` (written before the arg-sentinel piece ran, see FINDINGS.md):
// output[0] = the index where the first longest run of equal elements starts (0 for an empty array).
class LongestRunStartProblem : public ScalarFromArrayProblem {
public:
    explicit LongestRunStartProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
