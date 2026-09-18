#include "problems/TspProblem.hpp"

#include <algorithm>
#include <vector>

#include "core/Random.hpp"

namespace algosearch {

namespace {
constexpr int kInf = 1 << 20;

// Exact Held-Karp bitmask DP: dp[mask][i] = min cost of a path starting at
// vertex 0, visiting exactly the vertex set `mask` (which always contains
// bit 0), and ending at vertex i. Host-side ground truth only -- this is
// the checker, never a program the search is asked to find.
int tspCost(int v, const std::vector<int>& matrix) {
    if (v <= 0) return -1;
    if (v == 1) return 0;
    int full = 1 << v;
    std::vector<std::vector<int>> dp(static_cast<size_t>(full), std::vector<int>(static_cast<size_t>(v), kInf));
    dp[1][0] = 0;
    for (int mask = 1; mask < full; ++mask) {
        if (!(mask & 1)) continue;
        for (int i = 0; i < v; ++i) {
            if (!(mask & (1 << i))) continue;
            int cur = dp[static_cast<size_t>(mask)][static_cast<size_t>(i)];
            if (cur == kInf) continue;
            for (int j = 0; j < v; ++j) {
                if (mask & (1 << j)) continue;
                int w = matrix[static_cast<size_t>(i) * v + j];
                if (w <= 0) continue;
                int nmask = mask | (1 << j);
                int cand = cur + w;
                if (cand < dp[static_cast<size_t>(nmask)][static_cast<size_t>(j)]) dp[static_cast<size_t>(nmask)][static_cast<size_t>(j)] = cand;
            }
        }
    }
    int full_mask = full - 1;
    int best = kInf;
    for (int i = 1; i < v; ++i) {
        int cur = dp[static_cast<size_t>(full_mask)][static_cast<size_t>(i)];
        if (cur == kInf) continue;
        int w = matrix[static_cast<size_t>(i) * v + 0];
        if (w <= 0) continue;
        best = std::min(best, cur + w);
    }
    return best == kInf ? -1 : best;
}

std::vector<int> assemble(int v, const std::vector<int>& matrix) {
    std::vector<int> arr;
    arr.reserve(static_cast<size_t>(1) + static_cast<size_t>(4 * v) + matrix.size());
    arr.push_back(v);
    // Scratch: 4*V cells, sized for the larger of the two skeletons
    // (branch-and-bound's four length-V arrays); brute force uses only the
    // first 2*(V-1) of these. Always program-initialized.
    for (int i = 0; i < 4 * v; ++i) arr.push_back(0);
    arr.insert(arr.end(), matrix.begin(), matrix.end());
    return arr;
}

std::vector<int> randomWeightedMatrix(int v, double edgeProb, int maxWeight, Random& rng) {
    std::vector<int> m(static_cast<size_t>(v) * v, 0);
    for (int i = 0; i < v; ++i) {
        for (int j = 0; j < v; ++j) {
            if (i == j) continue;
            if (rng.chance(edgeProb)) m[static_cast<size_t>(i) * v + j] = 1 + rng.uniformInt(0, maxWeight - 1);
        }
    }
    return m;
}

void addInstance(int v, const std::vector<int>& matrix, std::vector<Instance>& out) {
    auto arr = assemble(v, matrix);
    out.push_back({arr, {tspCost(v, matrix)}});
}

// Structural variety a random sample rarely hits on its own, same
// discipline as ShortestPathProblem's addStructuralInstances.
void addStructuralInstances(int v, std::vector<Instance>& out) {
    if (v < 1) return;
    // Single cycle 0->1->...->(v-1)->0, weight 1 per edge, nothing else:
    // the only Hamiltonian tour, cost v.
    if (v >= 1) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 1 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        if (v >= 2) m[static_cast<size_t>(v - 1) * v + 0] = 1;
        addInstance(v, m, out);
    }
    // Complete graph, uniform weight 1: every permutation is a tour of
    // cost v -- tests that a program doesn't need to compare tour costs
    // when they're all equal, but must still find *a* valid tour.
    if (v >= 2) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i < v; ++i)
            for (int j = 0; j < v; ++j)
                if (i != j) m[static_cast<size_t>(i) * v + j] = 1;
        addInstance(v, m, out);
    }
    // Two competing Hamiltonian cycles of different cost: the cheap cycle
    // 0->1->...->(v-1)->0 at weight 1 per edge, PLUS an expensive direct
    // shortcut 0->(v-1) that is part of no cheaper tour on its own --
    // guards against a program that greedily takes the first edge it
    // finds instead of evaluating full tours.
    if (v >= 4) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 1 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        m[static_cast<size_t>(v - 1) * v + 0] = 1;
        m[static_cast<size_t>(0) * v + (v - 1)] = 50;
        addInstance(v, m, out);
    }
    // Forward chain exists but no edge back to vertex 0: no Hamiltonian
    // cycle exists at all -- expect -1.
    if (v >= 2) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 1 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        addInstance(v, m, out);
    }
    // Completely disconnected: unreachable (-1), unless v==1 (trivial
    // single-vertex tour, cost 0).
    {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        addInstance(v, m, out);
    }
    // Missing one vertex's only entry edge: a Hamiltonian tour is
    // impossible even though most of the graph is well connected --
    // guards against a program that only checks total edge count.
    if (v >= 4) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i < v; ++i)
            for (int j = 0; j < v; ++j)
                if (i != j && j != v - 1) m[static_cast<size_t>(i) * v + j] = 1;
        addInstance(v, m, out);
    }
}
}  // namespace

TspProblem::TspProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

std::vector<Instance> TspProblem::generateInstances(int level) const {
    int v = level;
    std::vector<Instance> instances;
    addStructuralInstances(v, instances);
    Random rng(seed_ + static_cast<uint64_t>(v) * 2654435761ULL);
    int samples = std::max(1, randomInstancesPerLevel_ / 4);
    for (int s = 0; s < samples; ++s) {
        double p = 0.3 + 0.5 * (static_cast<double>(s % 4) / 4.0);  // TSP needs denser graphs to have any tour at all
        addInstance(v, randomWeightedMatrix(v, p, 9, rng), instances);
    }
    return instances;
}

std::string TspProblem::describeLevel(int level) const { return "vertices V = " + std::to_string(level); }

int TspProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> TspProblem::generateAdversarialInstances(int level) const {
    if (level < 1) return {};
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);
    for (int s = 0; s < 8; ++s) {
        double p = 0.25 + 0.6 * (static_cast<double>(s % 5) / 5.0);
        addInstance(level, randomWeightedMatrix(level, p, 20, rng), instances);
    }
    return instances;
}

bool TspProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int v = input[0];
    if (v < 1) return false;
    size_t expectedLen = static_cast<size_t>(1) + static_cast<size_t>(4 * v) + static_cast<size_t>(v) * v;
    if (input.size() != expectedLen) return false;
    std::vector<int> matrix(input.begin() + 1 + 4 * v, input.end());
    return output[0] == tspCost(v, matrix);
}

}  // namespace algosearch
