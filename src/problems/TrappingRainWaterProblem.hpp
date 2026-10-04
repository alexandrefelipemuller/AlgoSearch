#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `trapping_rain_water` (written after the library freeze, see FINDINGS.md):
// output[0] = the water trapped by bars of heights arr: sum of min(max left incl. i, max right incl. i) - arr[i]. Classic (prefix/suffix maxima or two pointers).
class TrappingRainWaterProblem : public ScalarFromArrayProblem {
public:
    explicit TrappingRainWaterProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
