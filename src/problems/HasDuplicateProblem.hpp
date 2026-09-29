#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 39 post-freeze problem `has_duplicate` (written after the combinator axes
// were frozen in commit fb0e539): output[0] = 1 if some value occurs at least twice, else 0.
class HasDuplicateProblem : public ScalarFromArrayProblem {
public:
    explicit HasDuplicateProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
