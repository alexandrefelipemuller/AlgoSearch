#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Dynamic Time Warping / global sequence-alignment distance between two
// equal-length integer sequences A and B: the classic O(n^2) DP,
//   table[0][0] = 0; table[i][0] = table[0][j] = INF for i,j >= 1;
//   table[i][j] = |A[i-1]-B[j-1]| + min(table[i-1][j], table[i][j-1],
//                                        table[i-1][j-1])
// answer = table[n][n]. Chosen (over free-endpoint DTW) so the DP has the
// same "every cell filled exactly once, dependencies strictly above/left/
// diag" shape as edit distance -- SETH-hardness results (Bringmann &
// Kunnemann 2015 for Frechet distance; Backurs & Indyk 2015 for edit
// distance; Abboud/Backurs/Vassilevska-Williams 2015 for LCS/DTW) all say
// no O(n^{2-eps}) algorithm exists for ANY of these unless SETH fails --
// so O(n^2) is conjectured optimal, not just "the best known so far" the
// way earlier problems in this project (gcd, shortest_path) were.
//
// Mini-C layout (no native 2D arrays, so the DP table is flattened
// row-major into a scratch region the program itself must initialize,
// same convention as GraphReachabilityProblem/ShortestPathProblem):
//   input[0]                        = n (sequence length, both A and B)
//   input[1 .. (n+1)*(n+1)]         = scratch region, always 0 -- the
//                                      program's own DP table; table[i][j]
//                                      lives at input[1 + i*(n+1) + j].
//   input[1+(n+1)^2 .. +n-1]        = sequence A (n values)
//   input[1+(n+1)^2+n .. +2n-1]     = sequence B (n values)
// Total length: 1 + (n+1)*(n+1) + 2*n.
//
// Output: output[0] = table[n][n], the DTW distance.
class DtwProblem : public Problem {
public:
    static constexpr int kInf = 100000;

    explicit DtwProblem(int exhaustiveMaxN = 5, int randomInstancesPerLevel = 40, int numVars = 7,
                         uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // The table region is non-shiftable scratch (must start at exactly 0 /
    // kInf) and n is a length metadata slot -- same rationale as
    // GraphReachabilityProblem/ShortestPathProblem's opt-out.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
