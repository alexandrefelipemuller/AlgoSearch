#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Classic 3SUM: do three DISTINCT indices i, j, k exist in the array such
// that arr[i] + arr[j] + arr[k] == 0?
//
// No target prefix (unlike ContainsProblem) -- the whole input array is the
// haystack, and 0 is the fixed sum to hit. Output convention matches the
// other decision problems: output[0] is 1 if such a triple exists, 0
// otherwise.
//
// Needs negative values to be answerable at all (three non-negative ints
// only sum to zero if all three are zero), so generateInstances crafts
// balanced positive/negative cases directly rather than relying on
// ArrayInstances' mostly-non-negative base arrays -- the same lesson
// ContainsProblem's addBothCases comment documents: instances must not
// share a structural regularity (like "answer is always no") a program
// could shortcut instead of doing the real O(n^2)/O(n^3) relational work.
class ThreeSumProblem : public Problem {
public:
    explicit ThreeSumProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 5, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
