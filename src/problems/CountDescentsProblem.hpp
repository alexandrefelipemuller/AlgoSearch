#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 40 post-freeze problem `count_descents` (written after combinator v2 was
// frozen in commit 77c4f77): output[0] = the number of i with arr[i-1] > arr[i].
class CountDescentsProblem : public ScalarFromArrayProblem {
public:
    explicit CountDescentsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
