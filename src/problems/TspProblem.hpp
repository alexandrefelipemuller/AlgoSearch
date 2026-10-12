#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Traveling Salesman Problem (decision/optimization form): minimum total
// weight of a Hamiltonian cycle starting and ending at vertex 0 and
// visiting every other vertex exactly once, or -1 if no such cycle exists
// (the graph need not be complete -- a missing edge is a missing edge).
//
// UPDATE: free/generic search (no skeleton) was tried first and confirmed
// stuck exactly as expected -- multiple runs (up to num-vars=80, beam=5000,
// candidates=20000, 30 CPU-minutes) never got past V=2 at ~8/14, with no
// improvement from more compute. Exact TSP has no fixed-point relaxation
// shortcut like reachability/shortest-path do (a visited-once constraint
// isn't monotone), so a dedicated skeleton was added:
// genTspBruteForceSkeletonBlock in ProgramGenerator.cpp, an iterative
// (non-recursive) Heap's-algorithm permutation enumeration -- the only
// brute-force shape that fits Mini-C's no-recursion, no-dynamic-structure
// constraints for an arbitrary V. This is intentionally exponential
// ((V-1)! permutations): a genuine brute-force solution, not a smarter
// polynomial algorithm -- proving the search CAN find brute-force TSP is
// the point, not proving it can find something better than brute force.
//
// UPDATE 2: a second, more advanced skeleton was added --
// genTspBranchAndBoundSkeletonBlock, an iterative (still no-recursion)
// depth-first search with pruning (cuts a partial path once its cost
// already reaches the best complete tour found so far), to A/B against
// plain brute force. It needs a differently-shaped, LARGER scratch region
// (four parallel arrays of length V instead of two of length V-1), so the
// scratch region below is now sized for the bigger of the two skeletons;
// the brute-force skeleton simply uses only the first 2*(V-1) cells of it.
//
// Same flat-array convention as ShortestPathProblem/GraphReachabilityProblem
// (row-major weight matrix, 0 = no edge, plus a scratch region the program
// must initialize itself):
//   input[0]              = V (vertex count)
//   input[1 .. 4*V]        = scratch, always 0. Shaped for whichever
//                            skeleton a candidate program uses:
//                              - brute force (genTspBruteForceSkeletonBlock)
//                                uses only the first 2*(V-1) cells: perm[k]
//                                at input[1+k] (a permutation of vertices
//                                1..V-1, fixing vertex 0 as the tour's
//                                start/end) and Heap's-algorithm counter
//                                c[k] at input[1+(V-1)+k].
//                              - branch-and-bound
//                                (genTspBranchAndBoundSkeletonBlock) uses
//                                the full 4*V: path[k] at input[1+k],
//                                used[k] at input[1+V+k], choice[k] (next
//                                candidate to try at DFS depth k, the
//                                iterative stand-in for a recursive call's
//                                local loop variable) at input[1+2*V+k],
//                                partialCost[k] at input[1+3*V+k].
//   input[1+4*V .. end]     = row-major weight matrix (input[...+i*V+j] is
//                            the weight of edge i->j, 0 = no edge)
// Total length: 1 + 4*V + V*V (for V==1, the "tour" is trivially just
// staying at vertex 0, cost 0, regardless of scratch contents).
//
// Output: output[0] is the minimum Hamiltonian cycle cost through vertex 0,
// or -1 if none exists. Ground truth is computed host-side via Held-Karp
// bitmask DP (exact, exponential, fine for the small V this project's
// exhaustive levels use -- this is the checker, not a generated program;
// it doesn't need to match the skeleton's algorithm, only its answer).
class TspProblem : public Problem {
public:
    // numVars default raised to 12: genTspBruteForceSkeletonBlock needs 11
    // distinct scalar variables (nv, size, base, k, i, t, cost, w,
    // prevVertex, valid, best) -- one more slot than that, matching the
    // existing convention (e.g. ShortestPathProblem needs numVars >= 7 for
    // its 6 variables) of the variable pool being numVars-1 wide.
    explicit TspProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 12,
                         uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    // Exact TSP is exponential by definition: verification stops at V = 10
    // (FINDINGS.md section 107b), past which no exact program fits the step limit.
    Instance generateBenchmarkInstance(int n, uint64_t seed) const override;
    static constexpr int maxDomainN = 10;
    int numVars() const override { return numVars_; }
    // Vertex count and matrix cells are non-shiftable metadata/weights in
    // fixed slots (same rationale as ShortestPathProblem) -- a uniform
    // shift would also corrupt the "0 = no edge" convention.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
