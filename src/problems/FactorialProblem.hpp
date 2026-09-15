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
// Correctness reference deliberately uses plain (possibly overflowing)
// `int` arithmetic, same reasoning as FibonacciProblem: modular arithmetic
// mod 2^32 is preserved whether multiplication is done natively or via
// repeated addition (both reduce to the same sequence of int additions
// mod 2^32), so a genuinely correct discovered program matches this
// reference bit-for-bit even past the point where the true factorial value
// no longer fits in 32 bits -- no need to cap the benchmarkable domain.
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

private:
    int exhaustiveMaxBound_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
