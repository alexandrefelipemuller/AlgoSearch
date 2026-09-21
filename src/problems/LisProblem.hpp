#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Held-out discovery benchmark problem (FINDINGS.md "Open threads" / section 24):
// length of the longest strictly increasing subsequence (LIS) of a sequence.
//
// Classic 1D DP: dp[i] = 1 + max(dp[j] for j < i with value[j] < value[i]).
// Section 24's catalogue predicts this needs the ONE primitive no existing
// skeleton provides: "read several already-computed table entries and take a
// max over them" -- every skeleton in ProgramGenerator.cpp reads at most one
// running accumulator, never a whole row of previously-filled cells. This
// problem is deliberately NOT in the skeleton library; it exists to measure
// whether the free search can assemble that shape unaided (the baseline this
// file's companion experiment records), then to serve as the held-out ruler
// for the divergence-engine and composite-primitives work approved by the
// plenary.
//
// Flat-array convention (same idea as GraphReachabilityProblem):
//   input[0]              = n, sequence length
//   input[1 .. n]         = the sequence values
//   input[n+1 .. 2n]      = scratch region, always 0 -- the program's own DP
//                            table (dp[i] lives at input[n+i]); NOT part of
//                            the sequence, just working memory.
// Total length: 2n + 1. Output convention: output[0] = LIS length.
class LisProblem : public Problem {
public:
    explicit LisProblem(int exhaustiveMaxN = 4, int randomInstancesPerLevel = 30, int numVars = 6, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // input[0] is a length, not a shiftable magnitude; scratch zeros encode
    // "uncomputed", which a uniform shift would destroy -- same opt-out as
    // GraphReachabilityProblem.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
