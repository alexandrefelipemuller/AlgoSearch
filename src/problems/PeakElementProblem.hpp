#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 114 external held-out problem `peak_element` (Tutorialspoint "Easy" array list, see FINDINGS.md):
// output[0] = the first index whose element is >= each existing neighbour (0 for an empty array).
class PeakElementProblem : public ScalarFromArrayProblem {
public:
    explicit PeakElementProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
