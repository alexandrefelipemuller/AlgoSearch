#include "problems/ContainsProblem.hpp"

#include <algorithm>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

ContainsProblem::ContainsProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
void addPresentAt(const std::vector<int>& haystack, size_t index, std::vector<Instance>& out) {
    std::vector<int> presentInput;
    presentInput.reserve(haystack.size() + 1);
    presentInput.push_back(haystack[index]);
    presentInput.insert(presentInput.end(), haystack.begin(), haystack.end());
    out.push_back({presentInput, {1}});
}

// Builds the canonical instances for one haystack: target present (at
// three different positions -- front, middle, back) and target absent
// (guaranteed, strictly larger than every haystack element).
//
// Present-at-front ONLY (this function's original form) turned out to be a
// serious data-generation bug, not a search-difficulty issue: since every
// single "present" training/held-out/benchmark instance this problem ever
// produced had its target sitting at haystack[0], "target == haystack[0]"
// is a perfect proxy for the presence label across the entire dataset --
// the search found and exploited exactly that shortcut (confirmed via the
// --benchmark tool's independent re-verification: a champion program whose
// entire logic was `if (arr[target_idx] == arr[1]) ...`, i.e. literally
// checking against haystack[0], generalized flawlessly across sizes and
// magnitude-shift stress tests because the data itself never contradicted
// it). No amount of held-out/adversarial testing catches this as long as
// every instance shares the same structural regularity. Testing all three
// positions per haystack removes any single fixed index as a viable
// shortcut, forcing genuine full-array logic.
void addBothCases(const std::vector<int>& haystack, std::vector<Instance>& out) {
    addPresentAt(haystack, 0, out);
    addPresentAt(haystack, haystack.size() / 2, out);
    addPresentAt(haystack, haystack.size() - 1, out);

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
