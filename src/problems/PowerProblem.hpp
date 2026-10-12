#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// base^exponent, for base >= 0 and exponent >= 0 (0^0 = 1, by convention).
//
// Same magnitude-based convention as FactorialProblem/GcdProblem: input =
// {base, exponent}, "level" is a joint magnitude range over both. Grammar
// note, same reasoning as FactorialProblem: Mini-C has no multiply
// primitive, so the generalizing solution needs multiplication via
// repeated addition (accumulate `result = result + result_so_far` isn't
// quite it either -- the natural shape is `result = 1; for i in
// 1..exponent: result = result * base`, and each `* base` step is itself
// a repeated-addition loop) -- a nested loop with an outer counter over
// the exponent and an inner accumulator, structurally the same shape
// FactorialProblem needs, hence the same numVars >= 5 default.
//
// Domain: base, exponent >= 0 with base^exponent in int (FINDINGS.md
// section 100). The reference is exact (64-bit); out-of-domain pairs are
// dropped from every instance set instead of compared mod 2^32, which
// only add-based multiplication matched (native Mul errors on overflow).
// The shift check is off: base + 997 leaves the domain at exponent 4.
class PowerProblem : public Problem {
public:
    explicit PowerProblem(int exhaustiveMaxBound = 6, int randomInstancesPerLevel = 40, int numVars = 5, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    Instance generateBenchmarkInstance(int n, uint64_t seed) const override;
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxBound_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
