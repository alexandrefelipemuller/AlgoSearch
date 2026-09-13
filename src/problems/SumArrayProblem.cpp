#include "problems/SumArrayProblem.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

SumArrayProblem::SumArrayProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
long trueSum(const std::vector<int>& values) {
    long total = 0;
    for (int v : values) total += v;
    return total;
}

Instance toSumInstance(std::vector<int> values) {
    std::vector<int> expected = {static_cast<int>(trueSum(values))};
    return {std::move(values), std::move(expected)};
}
}  // namespace

std::vector<Instance> SumArrayProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& values : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        instances.push_back(toSumInstance(std::move(values)));
    }
    if (n >= 2) {
        for (auto& values : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            instances.push_back(toSumInstance(std::move(values)));
        }
    }
    return instances;
}

std::string SumArrayProblem::describeLevel(int level) const { return "N = " + std::to_string(level); }

int SumArrayProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> SumArrayProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& values : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        instances.push_back(toSumInstance(std::move(values)));
    }
    return instances;
}

bool SumArrayProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    return output[0] == trueSum(input);
}

double SumArrayProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return 0.0;
    long target = trueSum(input);
    double diff = std::abs(static_cast<double>(output[0]) - static_cast<double>(target));
    // Scale by the sum of absolute values (the largest "sensible" sum
    // magnitude for this instance) so the credit is roughly comparable
    // across arrays with very different value ranges.
    double scale = 1.0;
    for (int v : input) scale += std::abs(static_cast<double>(v));
    return std::max(0.0, 1.0 - diff / scale);
}

}  // namespace algosearch
