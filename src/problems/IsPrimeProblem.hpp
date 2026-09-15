#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Is a single integer prime?
//
// Output convention: same as every other scalar-answer problem here --
// input[0] is the number to test, output[0] is 1 if prime, 0 otherwise.
// Unlike the array problems, the input array is always length 1: "level"
// does not mean array size here, it means the numeric range being tested
// (level 1 tests small numbers, higher levels test a wider range), so
// difficulty grows by magnitude instead of by array length.
//
// Grammar note: Mini-C has no multiply/mod (see ast/Expr.hpp), so a correct
// generalizing program has to test divisibility by repeated subtraction --
// for each candidate divisor d in [2, x-1], subtract d from a running
// remainder until it's < d, and reject if the remainder isn't 0 -- which
// needs a genuine nested loop (outer: candidate divisors: inner: repeated
// subtraction), structurally similar to the nested-loop pattern that first
// appeared for sorting at N=4. This is deliberately a harder step back than
// min/max/sum/contains for that reason.
class IsPrimeProblem : public Problem {
public:
    explicit IsPrimeProblem(int exhaustiveMaxBound = 30, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }

private:
    int exhaustiveMaxBound_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
