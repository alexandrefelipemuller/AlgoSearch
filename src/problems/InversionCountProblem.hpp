#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 39 post-freeze problem `inversion_count` (written after the combinator axes
// were frozen in commit fb0e539): output[0] = the number of pairs i < j with arr[i] > arr[j].
class InversionCountProblem : public ScalarFromArrayProblem {
public:
    explicit InversionCountProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
