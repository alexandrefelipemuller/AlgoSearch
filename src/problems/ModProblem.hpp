#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// a mod b (the remainder of a divided by b), for a >= 0, b >= 1.
//
// This exists as a CURRICULUM STEP, not a toy problem in its own right:
// its generalizing solution needs a loop containing `Sub(Var, Var)`
// between two arbitrary scalars (`a = a - b` repeated while a > b) --
// exactly the primitive shape identified (via the ALGOSEARCH_DEBUG_LANDSCAPE
// diagnostic in BeamSearch.cpp) as the rare piece GcdProblem's search can
// never stumble into unaided. Unlike GcdProblem, this needs no compound
// "!=" condition -- `while (a > b) { a = a - b; }` uses only Gt and Sub,
// primitives that were already reachable, just rarely combined this way
// (see the genValueExpr rebalance fix this shipped alongside). If a
// genuinely correct solution can be found here through honest,
// unassisted composition, the discovered program -- not a hand-authored
// one -- is what gets offered back as a reusable skeleton for harder
// problems that need the same core idea (gcd chief among them). That is
// the whole point: the search teaches itself the piece, nobody hand-codes
// the answer.
class ModProblem : public Problem {
public:
    explicit ModProblem(int exhaustiveMaxBound = 10, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    Instance generateBenchmarkInstance(int n, uint64_t seed) const override;

private:
    int exhaustiveMaxBound_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
