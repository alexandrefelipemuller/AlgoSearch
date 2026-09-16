#include "problems/CountProblem.hpp"

#include <algorithm>
#include <cmath>

#include "problems/ArrayInstances.hpp"

namespace algosearch {

CountProblem::CountProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int trueCount(int threshold, const std::vector<int>& haystack) {
    int count = 0;
    for (int v : haystack) {
        if (v > threshold) count++;
    }
    return count;
}

Instance toCountInstance(int threshold, const std::vector<int>& haystack) {
    std::vector<int> input;
    input.reserve(haystack.size() + 1);
    input.push_back(threshold);
    input.insert(input.end(), haystack.begin(), haystack.end());
    return {std::move(input), {trueCount(threshold, haystack)}};
}

// Three thresholds per haystack so a solution can't get away with only
// ever seeing "everything counts" or "nothing counts": below the minimum
// (count == n), at the maximum (count == 0, since > is strict), and
// halfway between (a genuine partial count).
void addCountVariants(const std::vector<int>& haystack, std::vector<Instance>& out) {
    int mn = *std::min_element(haystack.begin(), haystack.end());
    int mx = *std::max_element(haystack.begin(), haystack.end());
    out.push_back(toCountInstance(mn - 1, haystack));
    out.push_back(toCountInstance(mx, haystack));
    out.push_back(toCountInstance(mn + (mx - mn) / 2, haystack));
}
}  // namespace

std::vector<Instance> CountProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    for (auto& haystack : generateBaseArrays(n, exhaustiveMaxN_, randomInstancesPerLevel_, seed_)) {
        addCountVariants(haystack, instances);
    }
    if (n >= 2) {
        for (auto& haystack : generateStructuralArrays(n, seed_, /*seedOffset=*/555555ULL, /*wideRangeSamples=*/3)) {
            addCountVariants(haystack, instances);
        }
    }
    return instances;
}

std::string CountProblem::describeLevel(int level) const { return "haystack N = " + std::to_string(level); }

int CountProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> CountProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    for (auto& haystack : generateStructuralArrays(level, seed_, /*seedOffset=*/999983ULL, /*wideRangeSamples=*/8)) {
        addCountVariants(haystack, instances);
    }
    return instances;
}

bool CountProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() < 2) return false;
    int threshold = input[0];
    std::vector<int> haystack(input.begin() + 1, input.end());
    return output[0] == trueCount(threshold, haystack);
}

double CountProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() < 2) return 0.0;
    int threshold = input[0];
    std::vector<int> haystack(input.begin() + 1, input.end());
    int target = trueCount(threshold, haystack);
    double diff = std::abs(static_cast<double>(output[0]) - static_cast<double>(target));
    double scale = std::max<double>(1.0, static_cast<double>(haystack.size()));
    return std::max(0.0, 1.0 - diff / scale);
}

}  // namespace algosearch
