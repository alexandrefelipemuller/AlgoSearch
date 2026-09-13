#include "problems/ContainsProblem.hpp"

#include <algorithm>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

ContainsProblem::ContainsProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
// Builds the two canonical instances for one haystack: target present
// (guaranteed, picked from the haystack itself) and target absent
// (guaranteed, strictly larger than every haystack element).
void addBothCases(const std::vector<int>& haystack, std::vector<Instance>& out) {
    int presentTarget = haystack.front();
    std::vector<int> presentInput;
    presentInput.reserve(haystack.size() + 1);
    presentInput.push_back(presentTarget);
    presentInput.insert(presentInput.end(), haystack.begin(), haystack.end());
    out.push_back({presentInput, {1}});

    int absentTarget = *std::max_element(haystack.begin(), haystack.end()) + 1;
    std::vector<int> absentInput;
    absentInput.reserve(haystack.size() + 1);
    absentInput.push_back(absentTarget);
    absentInput.insert(absentInput.end(), haystack.begin(), haystack.end());
    out.push_back({absentInput, {0}});
}
}  // namespace

std::vector<Instance> ContainsProblem::generateInstances(int level) const {
    int n = level;  // haystack length
    std::vector<Instance> instances;
    for (auto& haystack : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        addBothCases(haystack, instances);
    }
    if (n >= 2) {
        for (auto& haystack : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            addBothCases(haystack, instances);
        }
    }
    return instances;
}

std::string ContainsProblem::describeLevel(int level) const { return "haystack N = " + std::to_string(level); }

int ContainsProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ContainsProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& haystack : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        addBothCases(haystack, instances);
    }
    return instances;
}

bool ContainsProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int target = input[0];
    bool contains = std::find(input.begin() + 1, input.end(), target) != input.end();
    return output[0] == (contains ? 1 : 0);
}

}  // namespace algosearch
