#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 117 problem `first_split_two_to_one` (written before the first-index and prefix-split pieces ran, see FINDINGS.md):
// output[0] = the first index i with sum(a[0..i-1]) == 2 * sum(a[i+1..n-1]), or -1.
class FirstSplitTwoToOneProblem : public ScalarFromArrayProblem {
public:
    explicit FirstSplitTwoToOneProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
