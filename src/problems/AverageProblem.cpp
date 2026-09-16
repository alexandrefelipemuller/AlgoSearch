#include "problems/AverageProblem.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "core/Random.hpp"
#include "problems/ArrayInstances.hpp"

namespace algosearch {

AverageProblem::AverageProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
long trueSum(const std::vector<int>& values) {
    long total = 0;
    for (int v : values) total += v;
    return total;
}

// Truncating integer division, matching what a repeated-subtraction loop
// computes -- unambiguous here because every value is non-negative.
int trueAverage(const std::vector<int>& values) {
    if (values.empty()) return 0;
    return static_cast<int>(trueSum(values) / static_cast<long>(values.size()));
}

Instance toAverageInstance(std::vector<int> values) {
    std::vector<int> expected = {trueAverage(values)};
    return {std::move(values), std::move(expected)};
}

// generateStructuralArrays() (shared with the other array problems)
// deliberately includes negative values, which would make "average" an
// ambiguous target without also teaching the search sign-handling --
// out of scope here (see this file's header comment). These are the same
// structural ideas (all-equal, ascending, descending, wide random range)
// restricted to non-negative values instead.
std::vector<std::vector<int>> nonNegativeStructuralArrays(int n, uint64_t seed, uint64_t seedOffset) {
    std::vector<std::vector<int>> arrays;
    if (n <= 0) return arrays;
    arrays.push_back(std::vector<int>(static_cast<size_t>(n), static_cast<int>(n)));  // all-equal
    std::vector<int> ascending(static_cast<size_t>(n));
    std::iota(ascending.begin(), ascending.end(), 0);
    arrays.push_back(ascending);
    std::vector<int> descending = ascending;
    std::reverse(descending.begin(), descending.end());
    arrays.push_back(std::move(descending));

    Random rng(seed + seedOffset + static_cast<uint64_t>(n) * 7919ULL);
    for (int s = 0; s < 3; ++s) {
        std::vector<int> wide(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) wide[static_cast<size_t>(i)] = rng.uniformInt(0, 10 * n + 10);
        arrays.push_back(std::move(wide));
    }
    return arrays;
}
}  // namespace

std::vector<Instance> AverageProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& values : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        instances.push_back(toAverageInstance(std::move(values)));
    }
    if (n >= 2) {
        for (auto& values : nonNegativeStructuralArrays(n, seed_, /*seedOffset=*/555555ULL)) {
            instances.push_back(toAverageInstance(std::move(values)));
        }
    }
    return instances;
}

std::string AverageProblem::describeLevel(int level) const { return "N = " + std::to_string(level); }

int AverageProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> AverageProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& values : nonNegativeStructuralArrays(level, seed_, /*seedOffset=*/999983ULL)) {
        instances.push_back(toAverageInstance(std::move(values)));
    }
    return instances;
}

bool AverageProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    return output[0] == trueAverage(input);
}

double AverageProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return 0.0;
    int target = trueAverage(input);
    double diff = std::abs(static_cast<double>(output[0]) - static_cast<double>(target));
    double scale = 1.0;
    for (int v : input) scale += std::abs(static_cast<double>(v));
    return std::max(0.0, 1.0 - diff / scale);
}

}  // namespace algosearch
