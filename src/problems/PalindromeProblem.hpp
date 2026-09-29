#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 39 post-freeze problem `palindrome` (written after the combinator axes
// were frozen in commit fb0e539): output[0] = 1 if the array reads the same forwards and backwards, else 0.
class PalindromeProblem : public ScalarFromArrayProblem {
public:
    explicit PalindromeProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
