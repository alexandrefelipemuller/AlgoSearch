#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 39 post-freeze problem `is_sorted` (written after the combinator axes
// were frozen in commit fb0e539): output[0] = 1 if the array is non-decreasing, else 0.
class IsSortedProblem : public ScalarFromArrayProblem {
public:
    explicit IsSortedProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
