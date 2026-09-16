#include "problems/PowerProblem.hpp"

#include "core/Random.hpp"

namespace algosearch {

PowerProblem::PowerProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int powerRef(int base, int exponent) {
    int result = 1;
    for (int i = 0; i < exponent; ++i) result *= base;
    return result;
}

int upperBound(int level) { return 2 + level; }

Instance toInstance(int base, int exponent) { return {{base, exponent}, {powerRef(base, exponent)}}; }
}  // namespace

std::vector<Instance> PowerProblem::generateInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    if (bound <= exhaustiveMaxBound_) {
        for (int base = 0; base <= bound; ++base) {
            for (int exponent = 0; exponent <= bound; ++exponent) instances.push_back(toInstance(base, exponent));
        }
    } else {
        Random rng(seed_ + static_cast<uint64_t>(level) * 1000003ULL);
        for (int base : {0, 1, 2, bound}) {
            for (int exponent : {0, 1, 2, bound}) instances.push_back(toInstance(base, exponent));
        }
        for (int i = 0; i < randomInstancesPerLevel_; ++i) {
            instances.push_back(toInstance(rng.uniformInt(0, bound), rng.uniformInt(0, bound)));
        }
    }
    return instances;
}

Instance PowerProblem::generateBenchmarkInstance(int n, uint64_t /*seed*/) const { return toInstance(2, n); }

std::string PowerProblem::describeLevel(int level) const {
    return "base, exponent in [0, " + std::to_string(upperBound(level)) + "]";
}

int PowerProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> PowerProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) instances.push_back(toInstance(rng.uniformInt(0, bound), rng.uniformInt(0, bound)));
    return instances;
}

bool PowerProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() != 2) return false;
    return output[0] == powerRef(input[0], input[1]);
}

}  // namespace algosearch
