#include "problems/FibonacciProblem.hpp"

#include "core/Random.hpp"

namespace algosearch {

FibonacciProblem::FibonacciProblem(int exhaustiveMaxBound, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxBound_(exhaustiveMaxBound), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {
int fibRef(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        int nb = a + b;
        int na = nb - a;
        a = na;
        b = nb;
    }
    return a;
}

int upperBound(int level) { return 5 + 5 * level; }

Instance toInstance(int n) { return {{n}, {fibRef(n)}}; }
}  // namespace

std::vector<Instance> FibonacciProblem::generateInstances(int level) const {
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

std::string FibonacciProblem::describeLevel(int level) const { return "n in [0, " + std::to_string(upperBound(level)) + "]"; }

int FibonacciProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> FibonacciProblem::generateAdversarialInstances(int level) const {
    int bound = upperBound(level);
    std::vector<Instance> instances;
    Random rng(seed_ + 999983ULL + static_cast<uint64_t>(level) * 7919ULL);
    for (int i = 0; i < 8; ++i) instances.push_back(toInstance(rng.uniformInt(0, bound)));
    return instances;
}

bool FibonacciProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.empty()) return false;
    return output[0] == fibRef(input[0]);
}

Instance FibonacciProblem::generateBenchmarkInstance(int n, uint64_t /*seed*/) const { return toInstance(n); }

}  // namespace algosearch
