#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// n! (n factorial), for n >= 0 (0! = 1! = 1).
//
// Same magnitude-based convention as IsPrimeProblem/GcdProblem/
// FibonacciProblem: input[0] = n, "level" is a magnitude range.
//
// Grammar note: this is the hardest of the magnitude problems added this
// session, since the generalizing solution needs multiplication, and
// Mini-C has no multiply primitive -- only Add/Sub (see ast/Expr.hpp). The
// standard fix, same idea as GCD's subtractive division, is multiplication
// via repeated addition: to compute p*i, add p to a fresh accumulator i
// times. Iterating that once per factor (i = 2..n) needs a genuinely
// NESTED loop (outer over the factor i, inner performing the repeated
// addition) -- structurally the same shape as IsPrimeProblem's nested
// divisor-loop skeleton, but with 4 scratch scalars (running product,
// outer counter, inner accumulator, inner counter) instead of 3, so it
// needs numVars >= 5 to have room (pass --num-vars 5; default is 4). See
// GenConfig::loopAccumulatorBias in ProgramGenerator.hpp for how the
// dedicated skeleton is wired in without a problem-specific flag.
//
// Domain: 0 <= n <= 12, the n whose n! fits in int (FINDINGS.md section
// 100). The reference is exact (64-bit); larger n are out of domain, not
// compared mod 2^32. The old wrapping reference only matched add-based
// multiplication, because native Mul reports overflow as an error. Levels
// cap at n = 12, benchmark instances past it are skipped, and the shift
// check is off (n + 997 is always out of domain).
class FactorialProblem : public Problem {
public:
    explicit FactorialProblem(int exhaustiveMaxBound = 10, int randomInstancesPerLevel = 40, int numVars = 5,
                               uint64_t seed = 42);

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
