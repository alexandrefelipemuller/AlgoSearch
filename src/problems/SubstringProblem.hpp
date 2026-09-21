#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Held-out discovery benchmark problem: does `pattern` occur as a contiguous
// substring of `text`?
//
// The canonical algorithm is the naive O(n*m) nested scan (two counting loops
// with two captured lengths and a per-start match flag). It recombines only
// the project's SHARED primitives (counting loop + captured scalar + sentinel
// flag) in an arrangement no existing skeleton builds -- the nearest shapes
// are `Membership` (single target, no position alignment) and `ThreeSum`
// (three indices, but no notion of adjacency between two sequences). Kept out
// of the skeleton library on purpose: it is the held-out ruler for whether
// the search can RECOMBINE known primitives into a new composite, as opposed
// to inventing a new primitive outright.
//
// Flat-array convention (folds both sequences into the one array):
//   input[0]              = n, text length
//   input[1 .. n]         = text
//   input[n+1]            = m, pattern length
//   input[n+2 .. n+1+m]   = pattern
// Total length: n + m + 2. Output convention: output[0] = 1 if the pattern
// occurs as a substring, else 0.
class SubstringProblem : public Problem {
public:
    explicit SubstringProblem(int exhaustiveMaxN = 5, int randomInstancesPerLevel = 30, int numVars = 6,
                              uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // input[0]/input[n+1] are lengths, not shiftable magnitudes.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
