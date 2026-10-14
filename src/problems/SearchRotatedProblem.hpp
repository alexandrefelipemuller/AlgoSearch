#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 114 external held-out problem `search_rotated` (Tutorialspoint "Easy" array list, see FINDINGS.md):
// output[0] = the first index i >= 1 with arr[i] == arr[0] (the key), or -1.
class SearchRotatedProblem : public ScalarFromArrayProblem {
public:
    explicit SearchRotatedProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
