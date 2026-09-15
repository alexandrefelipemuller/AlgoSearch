#include "problems/IsPrimeProblem.hpp"

#include "core/Random.hpp"

namespace algosearch {

IsPrimeProblem::IsPrimeProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
bool isPrimeRef(int x) {
    if (x < 2) return false;
    for (int d = 2; d * d <= x; ++d) {
        if (x % d == 0) return false;
    }
    return true;
}

int upperBound(int level) { return 5 + 5 * level; }

Instance toInstance(int x) { return {{x}, {isPrimeRef(x) ? 1 : 0}}; }
}  // namespace

std::vector<Instance> IsPrimeProblem::generateInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    if (bound <= exhaustiveMaxBound_) {
        for (int x = 0; x <= bound; ++x) instances.push_back(toInstance(x));
    } else {
        Random rng(seed_ + static_cast<uint64_t>(level) * 1000003ULL);
        // Guaranteed edge cases every level includes regardless of sampling.
        for (int x : {0, 1, 2, 3, bound - 1, bound}) instances.push_back(toInstance(x));
        for (int i = 0; i < randomInstancesPerLevel_; ++i) {
            instances.push_back(toInstance(rng.uniformInt(0, bound)));
        }
    }
    return instances;
}

std::string IsPrimeProblem::describeLevel(int level) const { return "range [0, " + std::to_string(upperBound(level)) + "]"; }

int IsPrimeProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> IsPrimeProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    // Perfect squares (a classic false-positive trap for sloppy trial
    // division) and small primes/composites near the boundary.
    for (int k = 2; k * k <= bound; ++k) instances.push_back(toInstance(k * k));
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) instances.push_back(toInstance(rng.uniformInt(0, bound)));
    return instances;
}

bool IsPrimeProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    return output[0] == (isPrimeRef(input[0]) ? 1 : 0);
}

}  // namespace algosearch
