#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Integer average of an array: floor(sum(values) / n), values restricted
// to non-negative integers (same domain-restriction reasoning as
// ModProblem/GcdProblem -- Mini-C has no native divide, so the
// generalizing solution needs division-via-repeated-subtraction, whose
// truncating semantics only stay unambiguous for non-negative operands;
// see ModProblem.hpp).
//
// What this adds over SumArrayProblem: TWO accumulators (a running sum,
// exactly like SumArrayProblem, plus a division loop at the end) rather
// than one -- `n` itself needs no separate counter, since the interpreter
// already seeds v0 with the array length before the program runs (see
// Interpreter::run), so a correct solution is "sum the array into some
// scalar, then repeatedly subtract v0 from it while counting how many
// times that succeeds."
class AverageProblem : public Problem {
public:
    explicit AverageProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

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
