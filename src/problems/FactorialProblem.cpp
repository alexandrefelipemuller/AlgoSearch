#include "problems/FactorialProblem.hpp"

#include <algorithm>

#include "core/Random.hpp"

namespace algosearch {

FactorialProblem::FactorialProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
// Exact n! in 64 bits; only called on the domain, where it fits in int.
long long factorialRef(int n) {
    long long result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}

// The largest in-domain n (12! fits in int, 13! does not).
const int maxDomainN = 12;

int upperBound(int level) { return std::min(3 + 2 * level, maxDomainN); }

Instance toInstance(int n) { return {{n}, {static_cast<int>(factorialRef(n))}}; }

}  // namespace

std::vector<Instance> FactorialProblem::generateInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    if (bound <= exhaustiveMaxBound_) {
        for (int n = 0; n <= bound; ++n) instances.push_back(toInstance(n));
    } else {
        Random rng(seed_ + static_cast<uint64_t>(level) * 1000003ULL);
        for (int n : {0, 1, 2, bound - 1, bound}) instances.push_back(toInstance(n));
        for (int i = 0; i < randomInstancesPerLevel_; ++i) instances.push_back(toInstance(rng.uniformInt(0, bound)));
    }
    return instances;
}

std::string FactorialProblem::describeLevel(int level) const { return "n in [0, " + std::to_string(upperBound(level)) + "]"; }

int FactorialProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> FactorialProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) instances.push_back(toInstance(rng.uniformInt(0, bound)));
    return instances;
}

bool FactorialProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    if (input[0] < 0 || input[0] > maxDomainN) return false;  // out of domain
    return output[0] == factorialRef(input[0]);
}

Instance FactorialProblem::generateBenchmarkInstance(int n, uint64_t /*seed*/) const {
    if (n < 0 || n > maxDomainN) return {{}, {}};  // out of domain: Benchmark skips it
    return toInstance(n);
}

}  // namespace algosearch
