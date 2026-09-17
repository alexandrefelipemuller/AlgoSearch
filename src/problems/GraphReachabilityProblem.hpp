#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Classic graph problem: given a directed graph on `level` vertices, is
// vertex (V-1) reachable from vertex 0 (multi-hop, not just a direct edge)?
//
// Mini-C has no pointers/structs, so the graph is folded into the one flat
// integer array by convention:
//   input[0]              = V (vertex count)
//   input[1 .. V]          = scratch region, always 0 -- the program's own
//                            "reached" bitset (reached[k] lives at
//                            input[1+k]); NOT part of the graph itself, just
//                            working memory a correct algorithm needs and
//                            Mini-C has nowhere else to put.
//   input[1+V .. 1+V+V*V-1] = row-major adjacency matrix (input[1+V+i*V+j]
//                            is 1 if there is an edge i -> j, else 0)
// Total length: 1 + V + V*V.
//
// Output convention matches the other decision problems: output[0] is 1 if
// V-1 is reachable from 0, else 0.
//
// Genuinely needs the arithmetic extension (FINDINGS.md section 16): the
// matrix's row-major indexing (i*V+j) needs a native multiply, unreachable
// before that existed. The canonical solution is an O(V^3) fixed-point
// relaxation (repeat V times: for every reached vertex i and every edge
// i->j, mark j reached) -- Mini-C has no queue/stack for real BFS/DFS, but
// this needs nothing but arrays and comparisons, and V rounds is always
// enough since the longest possible simple path has fewer than V edges.
class GraphReachabilityProblem : public Problem {
public:
    explicit GraphReachabilityProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 6,
                                       uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // Vertex count (input[0]) and every adjacency-matrix cell are
    // non-shiftable metadata/flags, not free-form magnitudes -- see
    // Problem::supportsMagnitudeShift's comment.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
