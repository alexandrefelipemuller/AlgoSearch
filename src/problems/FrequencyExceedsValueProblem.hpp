#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 112 problem `frequency_exceeds_value` (written before the distinct-aggregate piece ran, see FINDINGS.md):
// output[0] = the number of distinct values v occurring more than v times (0 for an empty array).
class FrequencyExceedsValueProblem : public ScalarFromArrayProblem {
public:
    explicit FrequencyExceedsValueProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
