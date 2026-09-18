#include "problems/ShortestPathProblem.hpp"

#include <algorithm>
#include <vector>

#include "core/Random.hpp"

namespace algosearch {

ShortestPathProblem::ShortestPathProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int shortestDistance(int v, const std::vector<int>& matrix) {
    if (v <= 1) return 0;
    std::vector<int> dist(static_cast<size_t>(v), ShortestPathProblem::kInf);
    dist[0] = 0;
    for (int round = 0; round < v - 1; ++round) {
        for (int i = 0; i < v; ++i) {
            if (dist[static_cast<size_t>(i)] == ShortestPathProblem::kInf) continue;
            for (int j = 0; j < v; ++j) {
                int w = matrix[static_cast<size_t>(i) * v + j];
                if (w <= 0) continue;
                int cand = dist[static_cast<size_t>(i)] + w;
                if (cand < dist[static_cast<size_t>(j)]) dist[static_cast<size_t>(j)] = cand;
            }
        }
    }
    int d = dist[static_cast<size_t>(v) - 1];
    return d == ShortestPathProblem::kInf ? -1 : d;
}

std::vector<int> assemble(int v, const std::vector<int>& matrix) {
    std::vector<int> arr;
    arr.reserve(static_cast<size_t>(1) + v + matrix.size());
    arr.push_back(v);
    for (int i = 0; i < v; ++i) arr.push_back(0);
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
    out.push_back({arr, {shortestDistance(v, matrix)}});
}

// Structural variety, same discipline as GraphReachabilityProblem's
// addStructuralInstances: cases a purely random sample rarely produces on
// its own, so a program can't shortcut past genuine multi-hop relaxation.
void addStructuralInstances(int v, std::vector<Instance>& out) {
    if (v < 1) return;
    // Chain 0->1->2->...->(v-1), each hop weight 1: shortest path is v-1,
    // only reachable via the full chain -- needs enough relaxation rounds.
    {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 1 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        addInstance(v, m, out);
    }
    // Direct edge 0->(v-1) with a large weight, PLUS a cheaper multi-hop
    // chain: the direct edge alone is not the shortest path, so a program
    // that just checks direct-edge weight would get this wrong.
    if (v >= 3) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        m[static_cast<size_t>(0) * v + (v - 1)] = 100;
        for (int i = 0; i + 1 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        addInstance(v, m, out);
    }
    // Direct edge 0->(v-1) only, nothing else: shortest path is that
    // single edge's weight.
    if (v >= 2) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        m[static_cast<size_t>(0) * v + (v - 1)] = 3;
        addInstance(v, m, out);
    }
    // Completely disconnected: unreachable (-1), unless v==1.
    {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        addInstance(v, m, out);
    }
    // Chain that stops one short of the target: unreachable by one missing
    // edge -- guards against a program that just checks vertex-0 degree.
    if (v >= 3) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 2 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        addInstance(v, m, out);
    }
    // Fully connected, uniform weight 1: shortest path is the direct edge.
    if (v >= 2) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i < v; ++i)
            for (int j = 0; j < v; ++j)
                if (i != j) m[static_cast<size_t>(i) * v + j] = 1;
        addInstance(v, m, out);
    }
}
}  // namespace

std::vector<Instance> ShortestPathProblem::generateInstances(int level) const {
    int v = level;
    std::vector<Instance> instances;
    addStructuralInstances(v, instances);
    Random rng(seed_ + static_cast<uint64_t>(v) * 2654435761ULL);
    int samples = std::max(1, randomInstancesPerLevel_ / 4);
    for (int s = 0; s < samples; ++s) {
        double p = 0.15 + 0.5 * (static_cast<double>(s % 4) / 4.0);  // vary density across samples
        addInstance(v, randomWeightedMatrix(v, p, 9, rng), instances);
    }
    return instances;
}

std::string ShortestPathProblem::describeLevel(int level) const { return "vertices V = " + std::to_string(level); }

int ShortestPathProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ShortestPathProblem::generateAdversarialInstances(int level) const {
    if (level < 1) return {};
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);
    for (int s = 0; s < 8; ++s) {
        double p = 0.1 + 0.7 * (static_cast<double>(s % 5) / 5.0);
        addInstance(level, randomWeightedMatrix(level, p, 20, rng), instances);
    }
    return instances;
}

bool ShortestPathProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int v = input[0];
    if (v < 1) return false;
    size_t expectedLen = static_cast<size_t>(1) + static_cast<size_t>(v) + static_cast<size_t>(v) * v;
    if (input.size() != expectedLen) return false;
    std::vector<int> matrix(input.begin() + 1 + v, input.end());
    return output[0] == shortestDistance(v, matrix);
}

}  // namespace algosearch
