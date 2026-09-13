#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Membership test: does a haystack array contain a given target value?
//
// The interpreter only ever hands a program one flat integer array, so the
// query is folded into that array by convention: input[0] is the target,
// input[1..] is the haystack (length = level). Output convention is the
// same as the other scalar problems: output[0] is 1 if the target is in
// the haystack, 0 otherwise.
//
// This is the "hardest" of the three step-back problems: unlike min/sum
// (pure aggregation), a correct program must relate one specific element
// (the target) to the rest of the array, which is structurally closer to
// the comparisons a real sort needs.
class ContainsProblem : public Problem {
public:
    explicit ContainsProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

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
