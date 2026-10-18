#include "problems/ContainsOrderedProblem.hpp"

#include <algorithm>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

ContainsOrderedProblem::ContainsOrderedProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
void addPresentAt(const std::vector<int>& haystack, size_t index, std::vector<Instance>& out) {
    std::vector<int> presentInput;
    presentInput.reserve(haystack.size() + 1);
    presentInput.push_back(haystack[index]);
    presentInput.insert(presentInput.end(), haystack.begin(), haystack.end());
    out.push_back({presentInput, {1}});
}

// Builds the canonical instances for one sorted haystack: target present
// (at front, middle, and back -- testing only one fixed position let the
// search learn "target == haystack[that one index]" as a perfect proxy for
// presence, since every training instance shared that same regularity; see
// the identical bug and fix in ContainsProblem.cpp) and target absent
// (strictly larger than every element, so it stays "off the end"
// regardless of sort direction).
void addBothCases(const std::vector<int>& haystack, std::vector<Instance>& out) {
    addPresentAt(haystack, 0, out);
    addPresentAt(haystack, haystack.size() / 2, out);
    addPresentAt(haystack, haystack.size() - 1, out);

    int absentTarget = *std::max_element(haystack.begin(), haystack.end()) + 1;
    std::vector<int> absentInput;
    absentInput.reserve(haystack.size() + 1);
    absentInput.push_back(absentTarget);
    absentInput.insert(absentInput.end(), haystack.begin(), haystack.end());
    out.push_back({absentInput, {0}});
}

// Sorts `values` both ways and adds both-direction instances -- never only
// ascending or only descending, so a one-ended scan can't be rewarded by
// data bias alone.
void addSortedBothDirections(std::vector<int> values, std::vector<Instance>& out) {
    if (values.empty()) return;
    std::sort(values.begin(), values.end());
    addBothCases(values, out);
    std::reverse(values.begin(), values.end());
    addBothCases(values, out);
}

// Section 63: the target at EVERY position of an evenly spaced haystack
// (0, 2, 4, ...), and absent in every gap between neighbors (the odd
// values) and at both ends, in both directions. addBothCases only probes
// the front/middle/back and "off the end", which let a binary-search
// decision that also moves right when 0 < (v - target) * dir < lo pass
// every generated instance (wrong on 794 of 1638 dense probes).
void addEveryPositionCases(int n, std::vector<Instance>& out) {
    std::vector<int> haystack(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) haystack[static_cast<size_t>(i)] = 2 * i;
    for (int pass = 0; pass < 2; ++pass) {
        for (size_t i = 0; i < haystack.size(); ++i) addPresentAt(haystack, i, out);
        for (int t = -1; t <= 2 * n - 1; t += 2) {
            std::vector<int> input = {t};
            input.insert(input.end(), haystack.begin(), haystack.end());
            out.push_back({input, {0}});
        }
        std::reverse(haystack.begin(), haystack.end());
    }
}
}  // namespace

std::vector<Instance> ContainsOrderedProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& haystack : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        addSortedBothDirections(std::move(haystack), instances);
    }
    if (n >= 2) {
        for (auto& haystack : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            addSortedBothDirections(std::move(haystack), instances);
        }
    }
    if (n >= 1) addEveryPositionCases(n, instances);
    return instances;
}

std::string ContainsOrderedProblem::describeLevel(int level) const { return "sorted haystack N = " + std::to_string(level); }

int ContainsOrderedProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ContainsOrderedProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& haystack : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        addSortedBothDirections(std::move(haystack), instances);
    }
    // Section 122: every haystack size up to `level` with the key at every position and in every gap. A run that
    // starts at --start-level 2 never trains on a one-element haystack, and a halving search that is wrong only
    // there (section 121c: three of eight "hits") passed the structural arrays above.
    for (int m = 1; m <= level; ++m) addEveryPositionCases(m, instances);
    return instances;
}

bool ContainsOrderedProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int target = input[0];
    bool contains = std::find(input.begin() + 1, input.end(), target) != input.end();
    return output[0] == (contains ? 1 : 0);
}

Instance ContainsOrderedProblem::generateBenchmarkInstance(int n, uint64_t seed) const {
    if (n <= 0) return {{}, {}};
    std::vector<int> haystack(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) haystack[static_cast<size_t>(i)] = i;
    // Vary the target's position by seed (front/middle/back) instead of a
    // single fixed index -- this instance is reused by both the timing
    // benchmark AND verifyGeneralization's correctness re-check, and a
    // fixed position previously let a program that only checks
    // haystack[that one index] pass generalization undetected (see the
    // comment in ContainsProblem.cpp's addBothCases for the full story).
    size_t index = static_cast<size_t>(seed % 3 == 0 ? 0 : (seed % 3 == 1 ? haystack.size() / 2 : haystack.size() - 1));
    int target = haystack[index];
    std::vector<int> input;
    input.reserve(haystack.size() + 1);
    input.push_back(target);
    input.insert(input.end(), haystack.begin(), haystack.end());
    return {input, {1}};
}

}  // namespace algosearch
