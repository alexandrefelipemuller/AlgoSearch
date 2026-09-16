#include "problems/ReverseProblem.hpp"

#include <algorithm>
#include <cmath>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

ReverseProblem::ReverseProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
Instance toReversedInstance(std::vector<int> values) {
    std::vector<int> expected(values.rbegin(), values.rend());
    return {std::move(values), std::move(expected)};
}
}  // namespace

std::vector<Instance> ReverseProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& values : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        instances.push_back(toReversedInstance(std::move(values)));
    }
    if (n >= 2) {
        for (auto& values : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            instances.push_back(toReversedInstance(std::move(values)));
        }
    }
    return instances;
}

std::string ReverseProblem::describeLevel(int level) const { return "N = " + std::to_string(level); }

int ReverseProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ReverseProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& values : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        instances.push_back(toReversedInstance(std::move(values)));
    }
    return instances;
}

bool ReverseProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return false;
    return std::equal(output.begin(), output.end(), input.rbegin());
}

// Fraction of positions already correct, gated to 0 if `output` isn't
// even a rearrangement of `input` -- same guard as SortingProblem's
// inversions score, for the same reason (a program that e.g. zeroes the
// whole array would otherwise score deceptively well by luck on some
// positions).
double ReverseProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return 0.0;
    size_t n = output.size();
    if (n == 0) return 1.0;

    std::vector<int> sortedOutput = output;
    std::vector<int> sortedInput = input;
    std::sort(sortedOutput.begin(), sortedOutput.end());
    std::sort(sortedInput.begin(), sortedInput.end());
    if (sortedOutput != sortedInput) return 0.0;

    int matches = 0;
    for (size_t i = 0; i < n; ++i) {
        if (output[i] == input[n - 1 - i]) matches++;
    }
    return static_cast<double>(matches) / static_cast<double>(n);
}

}  // namespace algosearch
