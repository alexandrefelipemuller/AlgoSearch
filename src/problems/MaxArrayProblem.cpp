#include "problems/MaxArrayProblem.hpp"

#include <algorithm>
#include <cmath>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

MaxArrayProblem::MaxArrayProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
Instance toMaxInstance(std::vector<int> values) {
    int maxVal = *std::max_element(values.begin(), values.end());
    std::vector<int> expected = {maxVal};
    return {std::move(values), std::move(expected)};
}
}  // namespace

std::vector<Instance> MaxArrayProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& values : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        instances.push_back(toMaxInstance(std::move(values)));
    }
    if (n >= 2) {
        for (auto& values : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            instances.push_back(toMaxInstance(std::move(values)));
        }
    }
    return instances;
}

std::string MaxArrayProblem::describeLevel(int level) const { return "N = " + std::to_string(level); }

int MaxArrayProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> MaxArrayProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& values : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        instances.push_back(toMaxInstance(std::move(values)));
    }
    return instances;
}

bool MaxArrayProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    return output[0] == *std::max_element(input.begin(), input.end());
}

double MaxArrayProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return 0.0;
    int trueMin = *std::min_element(input.begin(), input.end());
    int trueMax = *std::max_element(input.begin(), input.end());
    long range = static_cast<long>(trueMax) - static_cast<long>(trueMin);
    if (range == 0) return output[0] == trueMax ? 1.0 : 0.0;
    double diff = std::abs(static_cast<double>(output[0]) - static_cast<double>(trueMax));
    return std::max(0.0, 1.0 - diff / static_cast<double>(range));
}

}  // namespace algosearch
