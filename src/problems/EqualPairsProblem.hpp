#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 86 post-freeze problem `equal_pairs` (written after the first-occurrence freeze, see FINDINGS.md):
// output[0] = the number of pairs i < j with arr[i] == arr[j].
class EqualPairsProblem : public ScalarFromArrayProblem {
public:
    explicit EqualPairsProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
