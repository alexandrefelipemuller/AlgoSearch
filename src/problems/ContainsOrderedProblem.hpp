#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Same membership test as ContainsProblem (input[0] = target, input[1..] =
// haystack, output[0] = 1/0), but the haystack is ALWAYS sorted -- half the
// time ascending, half descending (never only one direction, so the search
// can't get away with a one-ended scan that happens to exploit whichever
// order it was only ever shown).
//
// The point: a plain linear scan is the only thing ContainsProblem's
// ungated data ever rewards, since nothing in an unsorted array lets a
// program stop early with confidence. A sorted array is a genuinely
// different, strictly easier-to-exploit structure -- once you know the
// haystack is monotonic, you can stop as soon as you pass where the target
// would be (still O(n) worst case, but early-exit), or -- if the search
// ever discovers a doubling/bisection shape -- do sub-linear comparisons.
// This problem exists to see whether the SAME generic search, with no new
// problem-specific bias, spontaneously finds that faster shape once the
// data actually rewards it, exactly the same way loopAccumulatorBias's
// templates only pay off when a problem's data rewards them.
//
// Caveat worth keeping in mind before expecting a textbook binary search:
// Mini-C's grammar (see ast/Expr.hpp) has no multiply/divide, so the
// classic `mid = (lo + hi) / 2` bisection midpoint is not directly
// expressible. A doubling/galloping search (step = step + step, needing
// only Add) is expressible and is also sub-linear, so that is a more
// plausible shape to actually see the search find here than exact binary
// search.
class ContainsOrderedProblem : public Problem {
public:
    explicit ContainsOrderedProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4,
                                     uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // Ascending-sorted haystack of size n, 0..n-1, target = the middle
    // element (always present) -- a stable, representative case for timing
    // growth, not a best/worst-case cherry-pick.
    Instance generateBenchmarkInstance(int n, uint64_t seed) const override;

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
