#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 55 post-freeze problem `ceiling_water` (written after the stateful pieces
// were frozen in commit b30674a): output[0] = water hanging below a ceiling of heights arr: the sum over i of arr[i] - max(min of arr[0..i], min of arr[i..n)), the upside-down trapping rain water.
class CeilingWaterProblem : public ScalarFromArrayProblem {
public:
    explicit CeilingWaterProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
