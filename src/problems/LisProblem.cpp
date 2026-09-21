#include "problems/LisProblem.hpp"

#include <algorithm>
#include <cmath>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

LisProblem::LisProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {

int lisLength(const std::vector<int>& values) {
    if (values.empty()) return 0;
    std::vector<int> dp(values.size(), 1);
    int best = 1;
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (values[j] < values[i]) dp[i] = std::max(dp[i], dp[j] + 1);
        }
        best = std::max(best, dp[i]);
    }
    return best;
}

// Assembles [n, values..., n zeros...] with the expected output.
Instance makeInstance(const std::vector<int>& values) {
    int n = static_cast<int>(values.size());
    std::vector<int> input;
    input.reserve(static_cast<size_t>(2) * n + 1);
    input.push_back(n);
    input.insert(input.end(), values.begin(), values.end());
    input.insert(input.end(), static_cast<size_t>(n), 0);
    return {std::move(input), {lisLength(values)}};
}

void addInstance(const std::vector<int>& values, std::vector<Instance>& out) { out.push_back(makeInstance(values)); }

}  // namespace

std::vector<Instance> LisProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    if (n <= 0) return instances;

    for (auto& values : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        addInstance(values, instances);
    }
    if (n >= 2) {
        for (auto& values : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            addInstance(values, instances);
        }
    }
    return instances;
}

std::string LisProblem::describeLevel(int level) const { return "sequence N = " + std::to_string(level); }

int LisProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> LisProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& values : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        addInstance(values, instances);
    }
    return instances;
}

bool LisProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int n = input[0];
    if (n < 1 || static_cast<size_t>(2) * n + 1 != input.size()) return false;
    std::vector<int> values(input.begin() + 1, input.begin() + 1 + n);
    return output[0] == lisLength(values);
}

double LisProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return 0.0;
    int n = input[0];
    if (n < 1 || static_cast<size_t>(2) * n + 1 != input.size()) return 0.0;
    std::vector<int> values(input.begin() + 1, input.begin() + 1 + n);
    int answer = lisLength(values);
    double distance = std::abs(output[0] - answer);
    return std::clamp(1.0 - distance / static_cast<double>(std::max(1, answer)), 0.0, 1.0);
}

}  // namespace algosearch
