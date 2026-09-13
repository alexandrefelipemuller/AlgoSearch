#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Sorting problem: given an array of integers, produce it sorted ascending.
//
// Level k uses arrays of size N = k. While N is small enough, the level is
// exhaustive over all permutations of {0, ..., N-1}; beyond a configurable
// threshold it falls back to a bounded number of random instances so that
// larger N remain tractable.
class SortingProblem : public Problem {
public:
    // exhaustiveMaxN: largest N for which all permutations are generated.
    // randomInstancesPerLevel: how many random instances to sample for N above that.
    // numVars: scalar variables available to generated programs.
    explicit SortingProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;

    // 1.0 - (inversions in `output`) / (max possible inversions), gated to
    // 0.0 if `output` isn't even a rearrangement of `input` (guards against
    // e.g. a program that zeroes the whole array, which is trivially
    // "inversion-free" but not a sort). Gives beam search a smooth signal
    // towards full sortedness instead of a flat pass/fail count.
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;

    int numVars() const override { return numVars_; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
