#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 113 problem `frequent_mode_or_none` (written before the arg-sentinel piece ran, see FINDINGS.md):
// output[0] = the first most frequent value if it occurs at least twice, else -1.
class FrequentModeOrNoneProblem : public ScalarFromArrayProblem {
public:
    explicit FrequentModeOrNoneProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
