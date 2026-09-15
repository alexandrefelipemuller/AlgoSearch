#include "problems/ContainsOrderedProblem.hpp"

#include <algorithm>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

ContainsOrderedProblem::ContainsOrderedProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
// Builds the two canonical instances for one sorted haystack: target
// present (picked from the haystack itself) and target absent (strictly
// larger than every element, so it stays "off the end" regardless of
// sort direction -- deliberately not testing the absent case at both ends,
// since generalization is judged by isSolution() re-checked at held-out
// time regardless, and this keeps the instance count from doubling again).
void addBothCases(const std::vector<int>& haystack, std::vector<Instance>& out) {
    int presentTarget = haystack[haystack.size() / 2];
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

// Sorts `values` both ways and adds both-direction instances -- never only
// ascending or only descending, so a one-ended scan can't be rewarded by
// data bias alone.
void addSortedBothDirections(std::vector<int> values, std::vector<Instance>& out) {
    if (values.empty()) return;
    std::sort(values.begin(), values.end());
    addBothCases(values, out);
    std::reverse(values.begin(), values.end());
    addBothCases(values, out);
}
}  // namespace

std::vector<Instance> ContainsOrderedProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& haystack : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        addSortedBothDirections(std::move(haystack), instances);
    }
    if (n >= 2) {
        for (auto& haystack : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            addSortedBothDirections(std::move(haystack), instances);
        }
    }
    return instances;
}

std::string ContainsOrderedProblem::describeLevel(int level) const { return "sorted haystack N = " + std::to_string(level); }

int ContainsOrderedProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ContainsOrderedProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& haystack : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        addSortedBothDirections(std::move(haystack), instances);
    }
    return instances;
}

bool ContainsOrderedProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    int target = input[0];
    bool contains = std::find(input.begin() + 1, input.end(), target) != input.end();
    return output[0] == (contains ? 1 : 0);
}

Instance ContainsOrderedProblem::generateBenchmarkInstance(int n, uint64_t /*seed*/) const {
    if (n <= 0) return {{}, {}};
    std::vector<int> haystack(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) haystack[static_cast<size_t>(i)] = i;
    int target = haystack[haystack.size() / 2];
    std::vector<int> input;
    input.reserve(haystack.size() + 1);
    input.push_back(target);
    input.insert(input.end(), haystack.begin(), haystack.end());
    return {input, {1}};
}

}  // namespace algosearch
