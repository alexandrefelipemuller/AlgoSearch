#include "problems/ThreeSumProblem.hpp"

#include <algorithm>
#include <array>

#include "core/Random.hpp"
#include "problems/ArrayInstances.hpp"

namespace algosearch {

ThreeSumProblem::ThreeSumProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
bool hasZeroSumTriple(const std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            for (int k = j + 1; k < n; ++k) {
                if (arr[i] + arr[j] + arr[k] == 0) return true;
            }
        }
    }
    return false;
}

// Forces a zero-sum triple into `arr` at positions {lo, mid, hi} (three
// different placement patterns are used across instances so no single
// fixed index is a viable shortcut -- the same bug ContainsProblem's
// addBothCases comment documents and fixes the same way).
void forceZeroSumTripleAt(std::vector<int>& arr, size_t i0, size_t i1, size_t i2, Random& rng) {
    int a = rng.uniformInt(1, 5 + static_cast<int>(arr.size()));
    int b = rng.uniformInt(1, 5 + static_cast<int>(arr.size()));
    arr[i0] = a;
    arr[i1] = b;
    arr[i2] = -(a + b);
}

// Nudges `arr` until it has NO zero-sum triple, without touching the
// positions used to embed the "yes" cases so the "no" instances stay drawn
// from the same distribution otherwise.
void removeAnyZeroSumTriple(std::vector<int>& arr) {
    int guard = 0;
    while (hasZeroSumTriple(arr) && guard < 10000) {
        arr[0] += 1;
        ++guard;
    }
}

void addBothCases(const std::vector<int>& base, size_t n, Random& rng, std::vector<Instance>& out) {
    // "yes": three placement patterns (front/middle/back triple) of an
    // embedded zero-sum triple.
    if (n >= 3) {
        size_t mid = n / 2;
        std::vector<std::array<size_t, 3>> patterns = {
            {0, 1, 2},
            {0, mid, n - 1},
            {n - 3, n - 2, n - 1},
        };
        for (auto& p : patterns) {
            if (p[0] == p[1] || p[1] == p[2] || p[0] == p[2]) continue;
            std::vector<int> yes = base;
            forceZeroSumTripleAt(yes, p[0], p[1], p[2], rng);
            out.push_back({yes, {1}});
        }
    }

    // "no": the base array itself, perturbed just enough to guarantee no
    // zero-sum triple survives.
    std::vector<int> no = base;
    removeAnyZeroSumTriple(no);
    out.push_back({no, {0}});
}
}  // namespace

std::vector<Instance> ThreeSumProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    if (n < 3) {
        // Not enough elements for any triple: the only honest label is
        // "no", drawn from whatever base arrays exist at this size.
        for (auto& arr : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
            instances.push_back({arr, {0}});
        }
        return instances;
    }

    Random rng(seed_ + static_cast<uint64_t>(n) * 2654435761ULL);
    for (auto& arr : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        addBothCases(arr, static_cast<size_t>(n), rng, instances);
    }
    for (auto& arr : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
        addBothCases(arr, static_cast<size_t>(n), rng, instances);
    }
    return instances;
}

std::string ThreeSumProblem::describeLevel(int level) const { return "array N = " + std::to_string(level); }

int ThreeSumProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ThreeSumProblem::generateAdversarialInstances(int level) const {
    if (level < 3) return {};
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 1);
    for (auto& arr : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        addBothCases(arr, static_cast<size_t>(level), rng, instances);
    }
    return instances;
}

bool ThreeSumProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    bool exists = hasZeroSumTriple(input);
    return output[0] == (exists ? 1 : 0);
}

}  // namespace algosearch
