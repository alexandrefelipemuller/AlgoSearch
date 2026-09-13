#include "problems/SortingProblem.hpp"

#include <algorithm>
#include <numeric>

#include "core/Random.hpp"

namespace algosearch {

SortingProblem::SortingProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

std::vector<Instance> SortingProblem::generateInstances(int level) const {
    int n = level;  // level k -> arrays of size N = k
    std::vector<Instance> instances;

    if (n <= exhaustiveMaxN_) {
        std::vector<int> base(static_cast<size_t>(n));
        std::iota(base.begin(), base.end(), 0);
        std::vector<int> perm = base;
        std::sort(perm.begin(), perm.end());
        do {
            Instance inst;
            inst.input = perm;
            inst.expectedOutput = base;  // 0..n-1 sorted is itself
            instances.push_back(std::move(inst));
        } while (std::next_permutation(perm.begin(), perm.end()));
    } else {
        Random rng(seed_ + static_cast<uint64_t>(n) * 1000003ULL);
        for (int i = 0; i < randomInstancesPerLevel_; ++i) {
            std::vector<int> values(static_cast<size_t>(n));
            for (int j = 0; j < n; ++j) values[static_cast<size_t>(j)] = rng.uniformInt(0, n * 4);
            std::vector<int> expected = values;
            std::sort(expected.begin(), expected.end());
            instances.push_back({values, expected});
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
    int n = level;
    if (n <= 0) return {};
    std::vector<Instance> instances;
    auto add = [&](std::vector<int> values) {
        std::vector<int> expected = values;
        std::sort(expected.begin(), expected.end());
        instances.push_back({std::move(values), std::move(expected)});
    };

    add(std::vector<int>(static_cast<size_t>(n), 7));  // all-equal
    {
        std::vector<int> dup(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) dup[static_cast<size_t>(i)] = i / 2;  // heavy duplicates
        add(dup);
    }
    {
        std::vector<int> neg(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) neg[static_cast<size_t>(i)] = (i % 2 == 0 ? -1 : 1) * (i + 1);  // negatives
        add(neg);
    }
    {
        std::vector<int> sorted(static_cast<size_t>(n));
        std::iota(sorted.begin(), sorted.end(), 0);
        add(sorted);  // already sorted
    }
    {
        std::vector<int> reversed(static_cast<size_t>(n));
        std::iota(reversed.begin(), reversed.end(), 0);
        std::reverse(reversed.begin(), reversed.end());
        add(reversed);  // worst-case reverse order
    }
    {
        Random rng(seed_ + 999983ULL);
        std::vector<int> wide(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) wide[static_cast<size_t>(i)] = rng.uniformInt(-100, 100);  // wide range
        add(wide);
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
