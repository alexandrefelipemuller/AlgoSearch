#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 40 post-freeze problem `longest_run` (written after combinator v2 was
// frozen in commit 77c4f77): output[0] = the length of the longest run of equal adjacent values (0 for an empty array).
class LongestRunProblem : public ScalarFromArrayProblem {
public:
    explicit LongestRunProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
