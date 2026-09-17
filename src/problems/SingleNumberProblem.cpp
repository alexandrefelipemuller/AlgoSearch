#include "problems/SingleNumberProblem.hpp"

#include <algorithm>
#include <map>

#include "core/Random.hpp"

namespace algosearch {

SingleNumberProblem::SingleNumberProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
// Builds one instance: `pairCount` distinct paired values (each appearing
// twice) plus one unpaired value, with the unpaired value placed at
// `singlePos` -- varying that position across instances is the same
// "no fixed index becomes a shortcut" discipline ContainsProblem's
// addBothCases comment documents (and 3SUM's forceZeroSumTripleAt reuses).
std::vector<int> buildInstance(int pairCount, Random& rng, size_t singlePos, int& outSingleValue) {
    std::vector<int> values;
    values.reserve(static_cast<size_t>(pairCount) * 2 + 1);
    int nextVal = 1;
    for (int i = 0; i < pairCount; ++i) {
        values.push_back(nextVal);
        values.push_back(nextVal);
        nextVal += 1 + rng.uniformInt(0, 2);  // keep pair values spread out, distinct from each other
    }
    int singleValue = nextVal + 5;  // distinct from every pair value
    outSingleValue = singleValue;

    // Shuffle the paired values (Fisher-Yates via rng), then splice the
    // single value in at the requested position.
    for (size_t i = values.size(); i > 1; --i) {
        size_t j = static_cast<size_t>(rng.uniformInt(0, static_cast<int>(i) - 1));
        std::swap(values[i - 1], values[j]);
    }
    singlePos = std::min(singlePos, values.size());
    values.insert(values.begin() + static_cast<long>(singlePos), singleValue);
    return values;
}

void addInstancesForPairCount(int pairCount, Random& rng, std::vector<Instance>& out) {
    size_t arrLen = static_cast<size_t>(pairCount) * 2 + 1;
    std::vector<size_t> positions = {0, arrLen / 2, arrLen - 1};
    for (size_t pos : positions) {
        int singleValue = 0;
        auto arr = buildInstance(pairCount, rng, pos, singleValue);
        out.push_back({arr, {singleValue}});
    }
}
}  // namespace

std::vector<Instance> SingleNumberProblem::generateInstances(int level) const {
    int pairCount = level;
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(pairCount) * 2654435761ULL);
    int samples = std::max(1, randomInstancesPerLevel_ / 8);
    for (int s = 0; s < samples; ++s) {
        addInstancesForPairCount(pairCount, rng, instances);
    }
    return instances;
}

std::string SingleNumberProblem::describeLevel(int level) const {
    return "pairs = " + std::to_string(level) + " (array N = " + std::to_string(2 * level + 1) + ")";
}

int SingleNumberProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> SingleNumberProblem::generateAdversarialInstances(int level) const {
    if (level < 1) return {};
    std::vector<Instance> instances;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 7);
    for (int s = 0; s < 6; ++s) {
        addInstancesForPairCount(level, rng, instances);
    }
    return instances;
}

bool SingleNumberProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    std::map<int, int> counts;
    for (int v : input) counts[v]++;
    int oddValue = 0;
    int oddCount = 0;
    for (auto& [value, count] : counts) {
        if (count % 2 != 0) {
            oddValue = value;
            oddCount++;
        }
    }
    if (oddCount != 1) return false;  // instance itself isn't well-posed
    return output[0] == oddValue;
}

}  // namespace algosearch
