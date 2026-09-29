#include "problems/ScalarFromArrayProblem.hpp"

#include "problems/ArrayInstances.hpp"

namespace algosearch {

ScalarFromArrayProblem::ScalarFromArrayProblem(int numVars, uint64_t seed) : numVars_(numVars), seed_(seed) {}

Instance ScalarFromArrayProblem::toInstance(std::vector<int> values) const {
    int expected = answer(values);
    return {std::move(values), {expected}};
}

std::vector<Instance> ScalarFromArrayProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    if (n <= 0) return instances;
    int exhaustive = 1;
    for (int i = 0; i < n && exhaustive <= 81; ++i) exhaustive *= 3;
    if (exhaustive <= 81) {
        for (auto& values : allArraysOver(n, 0, 2)) instances.push_back(toInstance(std::move(values)));
    }
    for (auto& values : randomArraysOver(n, 0, n, 30, seed_ + static_cast<uint64_t>(n) * 1000003ULL)) {
        instances.push_back(toInstance(std::move(values)));
    }
    Random rng(seed_ + static_cast<uint64_t>(n) * 7919ULL + 3);
    for (auto& values : constructedArrays(n, rng)) instances.push_back(toInstance(std::move(values)));
    if (n >= 2) {
        for (auto& values : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            instances.push_back(toInstance(std::move(values)));
        }
    }
    return instances;
}

std::string ScalarFromArrayProblem::describeLevel(int level) const { return "N = " + std::to_string(level); }

bool ScalarFromArrayProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (input.empty() || output.size() != input.size()) return false;
    return output[0] == answer(input);
}

std::vector<Instance> ScalarFromArrayProblem::generateAdversarialInstances(int level) const {
    std::vector<Instance> instances;
    if (level <= 0) return instances;
    for (auto& values : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        instances.push_back(toInstance(std::move(values)));
    }
    Random rng(seed_ + static_cast<uint64_t>(level) * 104729ULL + 5);
    for (auto& values : constructedArrays(level, rng)) {
        for (int& v : values) v = v * 7 - 11;  // same label, values spread and partly negative
        instances.push_back(toInstance(std::move(values)));
    }
    return instances;
}

std::vector<std::vector<int>> ScalarFromArrayProblem::exhaustiveInputs(int n) const {
    if (n < 1 || n > 6) return {};
    return allArraysOver(n, -1, 2);
}

}  // namespace algosearch
