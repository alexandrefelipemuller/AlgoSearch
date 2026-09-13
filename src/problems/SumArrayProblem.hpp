#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Sum of all elements of an array of integers. Same output[0]-is-the-answer
// convention as MinArrayProblem; a bit harder in that a correct program
// must actually visit every element via a loop rather than a constant
// number of comparisons.
class SumArrayProblem : public Problem {
public:
    explicit SumArrayProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

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
