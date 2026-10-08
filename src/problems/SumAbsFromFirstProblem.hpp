#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 80 post-freeze problem `sum_abs_from_first` (written after the orthogonal-modifier freeze, see FINDINGS.md):
// output[0] = the sum of |arr[i] - arr[0]| over all i.
class SumAbsFromFirstProblem : public ScalarFromArrayProblem {
public:
    explicit SumAbsFromFirstProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
