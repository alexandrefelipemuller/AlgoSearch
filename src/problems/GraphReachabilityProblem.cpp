#include "problems/GraphReachabilityProblem.hpp"

#include <queue>
#include <vector>

#include "core/Random.hpp"

namespace algosearch {

GraphReachabilityProblem::GraphReachabilityProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
bool reachable(int v, const std::vector<int>& matrix) {
    if (v <= 1) return true;  // a single vertex trivially "reaches" itself
    std::vector<char> visited(static_cast<size_t>(v), 0);
    std::queue<int> q;
    visited[0] = 1;
    q.push(0);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int w = 0; w < v; ++w) {
            if (!visited[w] && matrix[static_cast<size_t>(u) * v + w]) {
                visited[w] = 1;
                q.push(w);
            }
        }
    }
    return visited[static_cast<size_t>(v) - 1] != 0;
}

// Assembles the full instance array from a vertex count and an already-built
// V*V adjacency matrix: [V, V zeros (scratch), flattened matrix].
std::vector<int> assemble(int v, const std::vector<int>& matrix) {
    std::vector<int> arr;
    arr.reserve(static_cast<size_t>(1) + v + matrix.size());
    arr.push_back(v);
    for (int i = 0; i < v; ++i) arr.push_back(0);
    arr.insert(arr.end(), matrix.begin(), matrix.end());
    return arr;
}

std::vector<int> randomMatrix(int v, double edgeProb, Random& rng) {
    std::vector<int> m(static_cast<size_t>(v) * v, 0);
    for (int i = 0; i < v; ++i) {
        for (int j = 0; j < v; ++j) {
            if (i == j) continue;
            if (rng.chance(edgeProb)) m[static_cast<size_t>(i) * v + j] = 1;
        }
    }
    return m;
}

void addInstance(int v, const std::vector<int>& matrix, std::vector<Instance>& out) {
    auto arr = assemble(v, matrix);
    int answer = reachable(v, matrix) ? 1 : 0;
    out.push_back({arr, {answer}});
}

// Structural variety, same discipline as every other array problem here
// (ContainsProblem's addBothCases, ThreeSumProblem's forced triples): cases
// a purely random edge-probability sample rarely produces on its own, so a
// program can't shortcut past genuine multi-hop reasoning.
void addStructuralInstances(int v, std::vector<Instance>& out) {
    if (v < 1) return;
    // Chain 0->1->2->...->(v-1): reachable, but ONLY via the full v-1 hop
    // path -- no direct edge, no shortcut -- the case that actually
    // requires the fixed-point relaxation to run enough rounds.
    {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 1 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        addInstance(v, m, out);
    }
    // Direct edge 0->(v-1) only, nothing else: reachable via a single hop.
    if (v >= 2) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        m[static_cast<size_t>(0) * v + (v - 1)] = 1;
        addInstance(v, m, out);
    }
    // Completely disconnected: never reachable (unless v==1).
    {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        addInstance(v, m, out);
    }
    // Chain that stops one short of the target: unreachable, but only by
    // one missing edge -- guards against a program that just checks "is
    // vertex 0 non-isolated" instead of tracing the actual path.
    if (v >= 3) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i + 2 < v; ++i) m[static_cast<size_t>(i) * v + (i + 1)] = 1;
        addInstance(v, m, out);
    }
    // Fully connected: reachable trivially.
    if (v >= 2) {
        std::vector<int> m(static_cast<size_t>(v) * v, 0);
        for (int i = 0; i < v; ++i)
            for (int j = 0; j < v; ++j)
                if (i != j) m[static_cast<size_t>(i) * v + j] = 1;
        addInstance(v, m, out);
    }
}
}  // namespace

std::vector<Instance> GraphReachabilityProblem::generateInstances(int level) const {
    int v = level;
    std::vector<Instance> instances;
    addStructuralInstances(v, instances);
    Random rng(seed_ + static_cast<uint64_t>(v) * 2654435761ULL);
    int samples = std::max(1, randomInstancesPerLevel_ / 4);
    for (int s = 0; s < samples; ++s) {
        double p = 0.15 + 0.5 * (static_cast<double>(s % 4) / 4.0);  // vary density across samples
        addInstance(v, randomMatrix(v, p, rng), instances);
    }
    return instances;
}

std::string GraphReachabilityProblem::describeLevel(int level) const { return "vertices V = " + std::to_string(level); }

int GraphReachabilityProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> GraphReachabilityProblem::generateAdversarialInstances(int level) const {
    if (level < 1) return {};
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);
    for (int s = 0; s < 8; ++s) {
        double p = 0.1 + 0.7 * (static_cast<double>(s % 5) / 5.0);
        addInstance(level, randomMatrix(level, p, rng), instances);
    }
    return instances;
}

bool GraphReachabilityProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int v = input[0];
    if (v < 1) return false;
    size_t expectedLen = static_cast<size_t>(1) + static_cast<size_t>(v) + static_cast<size_t>(v) * v;
    if (input.size() != expectedLen) return false;
    std::vector<int> matrix(input.begin() + 1 + v, input.end());
    return output[0] == (reachable(v, matrix) ? 1 : 0);
}

}  // namespace algosearch
