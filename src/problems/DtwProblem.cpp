#include "problems/DtwProblem.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/Random.hpp"

namespace algosearch {

DtwProblem::DtwProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int dtwDistance(int n, const std::vector<int>& a, const std::vector<int>& b) {
    if (n <= 0) return 0;
    std::vector<int> table(static_cast<size_t>(n + 1) * (n + 1), DtwProblem::kInf);
    table[0] = 0;
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= n; ++j) {
            int cost = std::abs(a[static_cast<size_t>(i) - 1] - b[static_cast<size_t>(j) - 1]);
            int up = table[static_cast<size_t>(i - 1) * (n + 1) + j];
            int left = table[static_cast<size_t>(i) * (n + 1) + (j - 1)];
            int diag = table[static_cast<size_t>(i - 1) * (n + 1) + (j - 1)];
            int best = std::min(up, std::min(left, diag));
            table[static_cast<size_t>(i) * (n + 1) + j] = cost + best;
        }
    }
    return table[static_cast<size_t>(n) * (n + 1) + n];
}

std::vector<int> assemble(int n, const std::vector<int>& a, const std::vector<int>& b) {
    std::vector<int> arr;
    size_t tableSize = static_cast<size_t>(n + 1) * (n + 1);
    arr.reserve(1 + tableSize + 2 * static_cast<size_t>(n));
    arr.push_back(n);
    for (size_t k = 0; k < tableSize; ++k) arr.push_back(0);
    arr.insert(arr.end(), a.begin(), a.end());
    arr.insert(arr.end(), b.begin(), b.end());
    return arr;
}

void addInstance(int n, const std::vector<int>& a, const std::vector<int>& b, std::vector<Instance>& out) {
    out.push_back({assemble(n, a, b), {dtwDistance(n, a, b)}});
}

std::vector<int> randomSeq(int n, int maxVal, Random& rng) {
    std::vector<int> v(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = rng.uniformInt(0, maxVal);
    return v;
}

// Structural variety, same discipline as ShortestPathProblem's
// addStructuralInstances: cases a purely random sample rarely produces,
// so a program can't shortcut past genuine cell-by-cell alignment.
void addStructuralInstances(int n, std::vector<Instance>& out) {
    if (n < 1) return;
    // Identical sequences: distance 0.
    {
        std::vector<int> a(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) a[static_cast<size_t>(i)] = i % 5;
        addInstance(n, a, a, out);
    }
    // Constant-shift sequences: every aligned pair costs the same shift.
    {
        std::vector<int> a(static_cast<size_t>(n)), b(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            a[static_cast<size_t>(i)] = i % 4;
            b[static_cast<size_t>(i)] = i % 4 + 3;
        }
        addInstance(n, a, b, out);
    }
    // A ascending, B descending: forces genuinely different alignments
    // across cells rather than a trivial diagonal-only shortcut.
    {
        std::vector<int> a(static_cast<size_t>(n)), b(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) {
            a[static_cast<size_t>(i)] = i;
            b[static_cast<size_t>(i)] = n - 1 - i;
        }
        addInstance(n, a, b, out);
    }
    // All-equal A, varying B.
    {
        std::vector<int> a(static_cast<size_t>(n), 5), b(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) b[static_cast<size_t>(i)] = i % 7;
        addInstance(n, a, b, out);
    }
    // Both all-zero: distance 0, degenerate case distinct from the
    // identical-nonzero-sequence case above.
    {
        std::vector<int> a(static_cast<size_t>(n), 0);
        addInstance(n, a, a, out);
    }
}
}  // namespace

std::vector<Instance> DtwProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    addStructuralInstances(n, instances);
    Random rng(seed_ + static_cast<uint64_t>(n) * 2654435761ULL);
    int samples = std::max(1, randomInstancesPerLevel_ / 4);
    for (int s = 0; s < samples; ++s) {
        int maxVal = 3 + (s % 4) * 3;  // vary value range across samples
        addInstance(n, randomSeq(n, maxVal, rng), randomSeq(n, maxVal, rng), instances);
    }
    return instances;
}

std::string DtwProblem::describeLevel(int level) const { return "sequence length n = " + std::to_string(level); }

int DtwProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> DtwProblem::generateAdversarialInstances(int level) const {
    if (level < 1) return {};
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);
    for (int s = 0; s < 8; ++s) {
        int maxVal = 5 + s * 3;
        addInstance(level, randomSeq(level, maxVal, rng), randomSeq(level, maxVal, rng), instances);
    }
    return instances;
}

bool DtwProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int n = input[0];
    if (n < 1) return false;
    size_t tableSize = static_cast<size_t>(n + 1) * (n + 1);
    size_t expectedLen = 1 + tableSize + 2 * static_cast<size_t>(n);
    if (input.size() != expectedLen) return false;
    std::vector<int> a(input.begin() + 1 + tableSize, input.begin() + 1 + tableSize + n);
    std::vector<int> b(input.begin() + 1 + tableSize + n, input.end());
    return output[0] == dtwDistance(n, a, b);
}

}  // namespace algosearch
