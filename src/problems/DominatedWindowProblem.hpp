#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Second held-out member of the "exists i, forall j: condition(i, j)"
// family (see FINDINGS.md section 32) -- deliberately structured to reuse
// SubstringProblem's exact flat two-sequence encoding and index arithmetic
// (same textIdx/patIdx affine forms) while swapping the per-(i,j) predicate
// from equality to strict elementwise dominance, so a search mechanism that
// solves both is demonstrating a genuine reset/AND/consume CAPABILITY, not
// a memorized encoding.
//
// Question: does `text` contain a contiguous window of length m, starting
// at some i, where EVERY element strictly exceeds the aligned `pattern`
// element (text[i+j] > pattern[j] for all j in [0, m))?
//
// Flat-array convention (identical to SubstringProblem.hpp):
//   input[0]              = n, text length
//   input[1 .. n]         = text
//   input[n+1]            = m, pattern length
//   input[n+2 .. n+1+m]   = pattern
// Total length: n + m + 2. Output convention: output[0] = 1 if such a
// window exists, else 0.
class DominatedWindowProblem : public Problem {
public:
    explicit DominatedWindowProblem(int exhaustiveMaxN = 5, int randomInstancesPerLevel = 30, int numVars = 8,
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
