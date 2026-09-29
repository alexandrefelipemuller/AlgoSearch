#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 40 post-freeze problem `strictly_increasing` (written after combinator v2 was
// frozen in commit 77c4f77): output[0] = 1 if every arr[i-1] < arr[i], else 0.
class StrictlyIncreasingProblem : public ScalarFromArrayProblem {
public:
    explicit StrictlyIncreasingProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
