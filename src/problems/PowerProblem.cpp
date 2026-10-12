#include "problems/PowerProblem.hpp"

#include <limits>

#include "core/Random.hpp"

namespace algosearch {

PowerProblem::PowerProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
// Exact base^exponent, or -1 once it no longer fits in int: out of domain.
long long powerRef(int base, int exponent) {
    long long result = 1;
    for (int i = 0; i < exponent; ++i) {
        result *= base;
        if (result > std::numeric_limits<int>::max()) return -1;
    }
    return result;
}

bool inDomain(int base, int exponent) { return base >= 0 && exponent >= 0 && powerRef(base, exponent) >= 0; }

int upperBound(int level) { return 2 + level; }

Instance toInstance(int base, int exponent) { return {{base, exponent}, {static_cast<int>(powerRef(base, exponent))}}; }

// Appends the instance only when it is in domain.
void addInDomain(std::vector<Instance>& out, int base, int exponent) {
    if (inDomain(base, exponent)) out.push_back(toInstance(base, exponent));
}
}  // namespace

std::vector<Instance> PowerProblem::generateInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    if (bound <= exhaustiveMaxBound_) {
        for (int base = 0; base <= bound; ++base) {
            for (int exponent = 0; exponent <= bound; ++exponent) addInDomain(instances, base, exponent);
        }
    } else {
        Random rng(seed_ + static_cast<uint64_t>(level) * 1000003ULL);
        for (int base : {0, 1, 2, bound}) {
            for (int exponent : {0, 1, 2, bound}) addInDomain(instances, base, exponent);
        }
        for (int i = 0; i < randomInstancesPerLevel_; ++i) {
            int base = rng.uniformInt(0, bound);
            addInDomain(instances, base, rng.uniformInt(0, bound));
        }
    }
    return instances;
}

Instance PowerProblem::generateBenchmarkInstance(int n, uint64_t /*seed*/) const {
    if (!inDomain(2, n)) return {{}, {}};  // out of domain: Benchmark skips it
    return toInstance(2, n);
}

std::string PowerProblem::describeLevel(int level) const {
    return "base, exponent in [0, " + std::to_string(upperBound(level)) + "]";
}

int PowerProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> PowerProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) {
        int base = rng.uniformInt(0, bound);
        addInDomain(instances, base, rng.uniformInt(0, bound));
    }
    return instances;
}

bool PowerProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() != 2) return false;
    if (!inDomain(input[0], input[1])) return false;  // out of domain
    return output[0] == powerRef(input[0], input[1]);
}

}  // namespace algosearch
