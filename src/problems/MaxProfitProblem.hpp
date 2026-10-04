#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 50 post-freeze problem `max_profit` (written after the library freeze, see FINDINGS.md):
// output[0] = max(0, max over i < j of arr[j] - arr[i]), the classic "best time to buy and sell stock". Classic (running minimum, or a triangular pair scan).
class MaxProfitProblem : public ScalarFromArrayProblem {
public:
    explicit MaxProfitProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
