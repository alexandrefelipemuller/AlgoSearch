#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// The n-th Fibonacci number, 0-indexed (fib(0)=0, fib(1)=1, fib(2)=1,
// fib(3)=2, ...).
//
// Same convention as IsPrimeProblem/GcdProblem: input is a single value
// (input[0] = n), "level" is a magnitude range rather than an array
// length, and "n" in generateBenchmarkInstance means the value tested, not
// a structural size.
//
// Grammar note: unlike GcdProblem/IsPrimeProblem, the generalizing
// solution here needs no compound condition and no nested loop -- the
// standard iterative recurrence (b=a+b; a=b-a; -- the a=b-a step recovers
// the OLD a using the just-updated b, avoiding a third temp variable) is a
// single flat loop using only Add/Sub, so it's actually the simplest of
// the four "hard" toy problems structurally.
//
// Correctness reference uses ordinary (possibly overflowing) `int`
// arithmetic deliberately, not a wider type: signed overflow wraps
// consistently under 2's-complement addition regardless of the exact
// sequence of additions performed (associativity holds mod 2^32), so a
// genuinely correct discovered program -- built entirely from int Add/Sub
// -- will match this reference bit-for-bit even at magnitudes where the
// "true" Fibonacci number is far larger than 32 bits. This is what makes
// it valid to benchmark timing growth at large n (see
// generateBenchmarkInstance) without needing to cap the domain.
class FibonacciProblem : public Problem {
public:
    explicit FibonacciProblem(int exhaustiveMaxBound = 20, int randomInstancesPerLevel = 40, int numVars = 4,
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
