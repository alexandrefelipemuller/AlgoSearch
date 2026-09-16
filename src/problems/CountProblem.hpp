#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Counts how many elements of a haystack array are strictly greater than a
// threshold. Same "query folded into the array" convention as
// ContainsProblem: input[0] is the threshold, input[1..] is the haystack
// (length = level). Output[0] is the count.
//
// What this adds over ContainsProblem/MinArrayProblem: a genuinely
// generalizing solution needs a COUNTER accumulator driven by a CONDITION
// inside the loop (`if (arr[i] > threshold) count = count + 1;`), not just
// a running min/max/sum or a single membership check -- the natural next
// step after those before something like AverageProblem's division needs
// a counter of its own for a different reason.
class CountProblem : public Problem {
public:
    explicit CountProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    double partialScore(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
