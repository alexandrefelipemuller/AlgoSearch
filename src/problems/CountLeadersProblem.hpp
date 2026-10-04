#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `count_leaders` (written after the library freeze, see FINDINGS.md):
// output[0] = the number of leaders, elements strictly greater than every element to their right (the last one always is). Classic (right-to-left running maximum).
class CountLeadersProblem : public ScalarFromArrayProblem {
public:
    explicit CountLeadersProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
