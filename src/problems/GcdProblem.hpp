#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Greatest common divisor of two positive integers.
//
// Output convention: input[0], input[1] are the two numbers (both >= 1);
// output[0] is gcd(input[0], input[1]). "Level" controls the numeric range
// both operands are drawn from, not an array length -- same convention as
// IsPrimeProblem.
//
// Grammar note: unlike IsPrimeProblem, this one is well within Mini-C's
// subtract/compare-only grammar -- the subtractive Euclidean algorithm
// (while a != b: if a > b then a = a - b else b = b - a; answer = a) needs
// only Sub, Gt, and a single while loop, no nested loops and no
// multiply/mod. Included as the "easy" half of this pair of new problems,
// to contrast against IsPrimeProblem's nested-loop requirement.
//
// partialScore matters here more than for the array problems: a first cut
// with only the inherited binary default got the beam stuck flat at 63/100
// within 0.2s and never moved for a full 10-minute run at the best-known
// sorting parameters (beam=800, candidates=6000) -- a lucky size-4 program
// (effectively "guess output[0]=input[1]", correct whenever b divides a)
// scores identically to every other wrong program, so nothing in the beam
// carries a signal toward the real algorithm. The distance-based version
// below (same style as MinArrayProblem) exists specifically to fix that.
class GcdProblem : public Problem {
public:
    explicit GcdProblem(int exhaustiveMaxBound = 12, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // "n" is the larger operand; paired with 1, the worst case for the
    // subtractive Euclidean algorithm (forces ~n subtraction steps) -- the
    // shape actually needed to see the true runtime growth, not a random
    // pair that might resolve in a couple of steps by luck.
    Instance generateBenchmarkInstance(int n, uint64_t seed) const override;

private:
    int exhaustiveMaxBound_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
