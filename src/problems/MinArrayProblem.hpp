#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Find the minimum value of an array of integers.
//
// Output convention (shared by every scalar-answer problem in this
// codebase, since the interpreter always returns an array the same size
// as the input): the answer is read from output[0]; the rest of the array
// is unconstrained. This is deliberately the simplest possible array
// problem -- a step back from sorting to validate that the same,
// unmodified search engine can discover something at all, and to shake
// out train/held-out generalization issues on a problem simple enough to
// reason about by hand.
class MinArrayProblem : public Problem {
public:
    explicit MinArrayProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

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
