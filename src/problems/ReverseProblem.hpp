#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Reverses an array in place: output[i] == input[n-1-i] for all i.
//
// Same full-array-output shape as SortingProblem, but the target
// permutation is fixed and known ahead of time rather than depending on
// value comparisons -- the natural solution is two pointers walking
// inward from both ends, swapping as they go (`swap(arr[lo], arr[hi]);
// lo = lo + 1; hi = hi - 1;`), which needs a loop CONDITION over two
// independently-moving indices instead of the single counter every other
// array problem here uses.
class ReverseProblem : public Problem {
public:
    explicit ReverseProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
