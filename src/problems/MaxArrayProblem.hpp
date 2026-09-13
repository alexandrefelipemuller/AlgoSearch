#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Find the maximum value of an array of integers. The mirror image of
// MinArrayProblem -- same output[0]-is-the-answer convention, same
// train/held-out instance discipline. Included specifically to compare
// against MinArrayProblem's discovered programs: if the search finds
// genuinely symmetric solutions (e.g. the same loop shape with > flipped
// to <), that's more evidence of real algorithmic generalization than
// coincidence.
class MaxArrayProblem : public Problem {
public:
    explicit MaxArrayProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

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
