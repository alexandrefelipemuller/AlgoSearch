#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `longest_increasing_run` (written after the library freeze, see FINDINGS.md):
// output[0] = the length of the longest strictly increasing contiguous run (0 for an empty array). Type 2 (running state that resets).
class LongestIncreasingRunProblem : public ScalarFromArrayProblem {
public:
    explicit LongestIncreasingRunProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
