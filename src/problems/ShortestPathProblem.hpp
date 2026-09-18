#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Weighted-graph shortest path: minimum total edge weight from vertex 0 to
// vertex (V-1) in a directed graph, or -1 if unreachable.
//
// Same flat-array convention as GraphReachabilityProblem, but the matrix
// cells are edge weights (0 = no edge, weight >= 1 = edge i->j with that
// cost) instead of 0/1 flags, and the scratch region is a `dist` array the
// program itself must initialize (dist[0] = 0, dist[i] = INF otherwise)
// before relaxing:
//   input[0]              = V (vertex count)
//   input[1 .. V]          = scratch region, always 0 -- the program's own
//                            `dist` array (dist[k] lives at input[1+k]).
//   input[1+V .. 1+V+V*V-1] = row-major weight matrix (input[1+V+i*V+j] is
//                            the weight of edge i->j, 0 meaning no edge).
// Total length: 1 + V + V*V.
//
// Output: output[0] is the shortest distance 0 -> V-1, or -1 if
// unreachable. All weights are non-negative, so Bellman-Ford-style
// relaxation (repeat V-1 times: for every edge i->j, dist[j] = min(dist[j],
// dist[i]+w[i][j])) converges in at most V-1 rounds, same rationale as
// GraphReachabilityProblem's V-round fixed point.
class ShortestPathProblem : public Problem {
public:
    static constexpr int kInf = 100000;

    explicit ShortestPathProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 7,
                                  uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // Vertex count and matrix cells are non-shiftable metadata/weights in
    // fixed slots (same rationale as GraphReachabilityProblem) -- a uniform
    // shift would also corrupt the "0 = no edge" convention.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
