#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Held-out discovery benchmark problem: linked-list cycle detection on a
// functional graph encoded in one flat array.
//
//   input[0]              = head index (0..n, where n = null / empty list)
//   input[1 .. n]         = next pointers for nodes 0..n-1; next[i] lives at
//                           input[1+i] and is in [0..n], with n meaning null.
// Total length: n + 1. Output convention: output[0] = 1 if the walk starting
// at head revisits a node before hitting null, else 0.
//
// The canonical algorithms are Floyd's tortoise-and-hare (two pointers moving
// at different speeds, compared for equality each round) or the
// pigeonhole/counting walk (advance at most n+1 steps; hitting null means
// acyclic). NEITHER shape exists in the skeleton library: the nearest is
// binary search's converging two-pointer (section 24's catalogue), which
// moves pointers toward each other by halving, not at different speeds, and
// graph_reachability's round-based relaxation, which never follows a single
// moving "cursor" through next-pointers at all. Kept out of the skeleton
// library on purpose as the held-out ruler for a genuinely NEW two-pointer
// shape.
class CycleDetectionProblem : public Problem {
public:
    explicit CycleDetectionProblem(int exhaustiveMaxN = 6, int randomInstancesPerLevel = 30, int numVars = 5,
                                   uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<std::vector<int>> exhaustiveInputs(int n) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }
    // head and every next pointer are non-shiftable indices.
    bool supportsMagnitudeShift() const override { return false; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
