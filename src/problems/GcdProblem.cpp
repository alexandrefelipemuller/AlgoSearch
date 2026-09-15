#include "problems/GcdProblem.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "core/Random.hpp"

namespace algosearch {

GcdProblem::GcdProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int upperBound(int level) { return 5 + 5 * level; }

Instance toInstance(int a, int b) { return {{a, b}, {static_cast<int>(std::gcd(a, b))}}; }
}  // namespace

std::vector<Instance> GcdProblem::generateInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    if (bound <= exhaustiveMaxBound_) {
        for (int a = 1; a <= bound; ++a) {
            for (int b = 1; b <= bound; ++b) instances.push_back(toInstance(a, b));
        }
    } else {
        Random rng(seed_ + static_cast<uint64_t>(level) * 1000003ULL);
        // Guaranteed structural cases: equal operands, one operand = 1,
        // exact multiples (fast-terminating and slow-terminating cases).
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

std::string GcdProblem::describeLevel(int level) const { return "operands in [1, " + std::to_string(upperBound(level)) + "]"; }

int GcdProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> GcdProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    // Coprime pairs (worst case for the subtractive algorithm: many steps)
    // and wide-gap pairs the base/random sets rarely produce.
    instances.push_back(toInstance(bound, bound > 1 ? bound - 1 : 1));  // consecutive -> coprime
    instances.push_back(toInstance(1, 1));
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) instances.push_back(toInstance(rng.uniformInt(1, bound), rng.uniformInt(1, bound)));
    return instances;
}

bool GcdProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() != 2) return false;
    return output[0] == static_cast<int>(std::gcd(input[0], input[1]));
}

double GcdProblem::partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() != 2) return 0.0;
    int trueGcd = static_cast<int>(std::gcd(input[0], input[1]));
    int maxOperand = std::max(input[0], input[1]);
    if (maxOperand == 0) return output[0] == trueGcd ? 1.0 : 0.0;
    double diff = std::abs(static_cast<double>(output[0]) - static_cast<double>(trueGcd));
    return std::max(0.0, 1.0 - diff / static_cast<double>(maxOperand));
}

}  // namespace algosearch
