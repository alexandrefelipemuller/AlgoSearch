#include "problems/SortingProblem.hpp"

#include <algorithm>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

SortingProblem::SortingProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
Instance toSortedInstance(std::vector<int> values) {
    std::vector<int> expected = values;
    std::sort(expected.begin(), expected.end());
    return {std::move(values), std::move(expected)};
}
}  // namespace

std::vector<Instance> SortingProblem::generateInstances(int level) const {
    int n = level;  // level k -> arrays of size N = k
    std::vector<Instance> instances;

    for (auto& values : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        instances.push_back(toSortedInstance(std::move(values)));
    }

    // Permutations of {0..n-1} alone let a program "sort" by exploiting
    // that the values ARE the correct target indices (e.g. using arr[i] to
    // index directly into the sorted position), without doing anything
    // resembling comparison-based sorting. Folding duplicates/negatives/
    // wide-range/already-sorted/reversed instances into the actual scored
    // set closes that loophole: full marks on a level now requires
    // generalizing past "the input happens to be 0..n-1 shuffled".
    if (n >= 2) {
        for (auto& values : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            instances.push_back(toSortedInstance(std::move(values)));
        }
    }
    return instances;
}

std::string SortingProblem::describeLevel(int level) const {
    int n = level;
    return "N = " + std::to_string(n);
}

int SortingProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> SortingProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    // Different seed offset than generateInstances()'s structural
    // instances, and more wide-range random samples, so this stays a
    // genuinely held-out check rather than re-measuring training data.
    std::vector<Instance> instances;
    for (auto& values : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        instances.push_back(toSortedInstance(std::move(values)));
    }
    return instances;
}

bool SortingProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return false;
    std::vector<int> expected = input;
    std::sort(expected.begin(), expected.end());
    return output == expected;
}

double SortingProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return 0.0;
    size_t n = output.size();
    if (n <= 1) return 1.0;

    // Must be a genuine rearrangement of the input -- otherwise inversions
    // could look great (e.g. all zeros) while being a completely wrong
    // answer.
    std::vector<int> sortedOutput = output;
    std::vector<int> sortedInput = input;
    std::sort(sortedOutput.begin(), sortedOutput.end());
    std::sort(sortedInput.begin(), sortedInput.end());
    if (sortedOutput != sortedInput) return 0.0;

    long inversions = 0;
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (output[i] > output[j]) inversions++;
        }
    }
    long maxInversions = static_cast<long>(n) * static_cast<long>(n - 1) / 2;
    return 1.0 - static_cast<double>(inversions) / static_cast<double>(maxInversions);
}

}  // namespace algosearch
