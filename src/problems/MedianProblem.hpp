#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `median` (written after the library freeze, see FINDINGS.md):
// output[0] = the lower median, sorted(arr)[(n-1)/2]. Type 1 (sort, then read one position).
class MedianProblem : public ScalarFromArrayProblem {
public:
    explicit MedianProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
