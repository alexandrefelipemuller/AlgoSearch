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

bool SortingProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size()) return false;
    std::vector<int> expected = input;
    std::sort(expected.begin(), expected.end());
    return output == expected;
}

}  // namespace algosearch
