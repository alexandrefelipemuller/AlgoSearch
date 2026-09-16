#include "problems/ModProblem.hpp"

#include <algorithm>
#include <cmath>

#include "core/Random.hpp"

namespace algosearch {

ModProblem::ModProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int upperBound(int level) { return 5 + 5 * level; }

Instance toInstance(int a, int b) { return {{a, b}, {a % b}}; }
}  // namespace

std::vector<Instance> ModProblem::generateInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    if (bound <= exhaustiveMaxBound_) {
        for (int a = 1; a <= bound; ++a) {
            for (int b = 1; b <= bound; ++b) instances.push_back(toInstance(a, b));
        }
    } else {
        Random rng(seed_ + static_cast<uint64_t>(level) * 1000003ULL);
        instances.push_back(toInstance(bound, bound));
        instances.push_back(toInstance(1, bound));
        instances.push_back(toInstance(bound, 1));
        instances.push_back(toInstance(bound, bound / 2 == 0 ? 1 : bound / 2));
        for (int i = 0; i < randomInstancesPerLevel_; ++i) {
            instances.push_back(toInstance(rng.uniformInt(1, bound), rng.uniformInt(1, bound)));
        }
    }
    return instances;
}

Instance ModProblem::generateBenchmarkInstance(int n, uint64_t /*seed*/) const { return toInstance(std::max(1, n), 1); }

std::string ModProblem::describeLevel(int level) const { return "operands in [1, " + std::to_string(upperBound(level)) + "]"; }

int ModProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> ModProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    instances.push_back(toInstance(bound, bound > 1 ? bound - 1 : 1));
    instances.push_back(toInstance(1, 1));
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) instances.push_back(toInstance(rng.uniformInt(1, bound), rng.uniformInt(1, bound)));
    return instances;
}

bool ModProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() != 2) return false;
    return output[0] == input[0] % input[1];
}

double ModProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() != 2) return 0.0;
    int trueMod = input[0] % input[1];
    int maxOperand = std::max(input[0], input[1]);
    if (maxOperand == 0) return output[0] == trueMod ? 1.0 : 0.0;
    double diff = std::abs(static_cast<double>(output[0]) - static_cast<double>(trueMod));
    return std::max(0.0, 1.0 - diff / static_cast<double>(maxOperand));
}

}  // namespace algosearch
