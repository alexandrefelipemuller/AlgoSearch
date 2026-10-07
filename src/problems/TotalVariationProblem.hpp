#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 71 post-freeze problem `total_variation` (written after the nested-aggregate freeze, see FINDINGS.md):
// output[0] = the sum of |arr[i] - arr[i-1]| over adjacent pairs. Classic (one adjacent pass).
class TotalVariationProblem : public ScalarFromArrayProblem {
public:
    explicit TotalVariationProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
