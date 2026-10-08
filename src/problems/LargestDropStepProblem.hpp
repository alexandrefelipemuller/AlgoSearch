#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 80 post-freeze problem `largest_drop_step` (written after the orthogonal-modifier freeze, see FINDINGS.md):
// output[0] = the largest arr[i-1] - arr[i] over adjacent pairs, which is negative when every step rises (0 when n < 2).
class LargestDropStepProblem : public ScalarFromArrayProblem {
public:
    explicit LargestDropStepProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
