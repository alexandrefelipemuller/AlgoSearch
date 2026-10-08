#pragma once

#include "problems/CycleDetectionProblem.hpp"

namespace algosearch {

// Section 77 post-freeze problems (written after the pointer-following
// freeze, see FINDINGS.md): other questions about the walk from the head on
// CycleDetectionProblem's encoding (input[0] = head, input[1 + i] = next of
// node i, n = null), on the same instances, relabeled by answer().
class ListQueryProblem : public CycleDetectionProblem {
public:
    explicit ListQueryProblem(int numVars = 8, uint64_t seed = 42) : CycleDetectionProblem(6, 30, numVars, seed) {}

    // The correct output[0] for `input`.
    virtual int answer(const std::vector<int>& input) const = 0;

    std::vector<Instance> generateInstances(int level) const override { return relabel(CycleDetectionProblem::generateInstances(level)); }
    std::vector<Instance> generateAdversarialInstances(int level) const override {
        return relabel(CycleDetectionProblem::generateAdversarialInstances(level));
    }
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override {
        return output.size() == input.size() && !output.empty() && output[0] == answer(input);
    }

protected:
    // The walk from the head: the nodes in order until null, stopping before
    // the first repeated node. `cyclic` is set when it stopped on a repeat.
    static std::vector<int> walk(const std::vector<int>& input, bool& cyclic);

private:
    std::vector<Instance> relabel(std::vector<Instance> instances) const {
        for (auto& inst : instances) inst.expectedOutput = {answer(inst.input)};
        return instances;
    }
};

// Nodes on the walk before null, or n + 1 when the walk never ends.
class WalkLengthProblem : public ListQueryProblem {
public:
    using ListQueryProblem::ListQueryProblem;
    int answer(const std::vector<int>& input) const override;
};

// 1 if the walk from the head visits node 0, else 0.
class VisitsZeroProblem : public ListQueryProblem {
public:
    using ListQueryProblem::ListQueryProblem;
    int answer(const std::vector<int>& input) const override;
};

// The number of nodes on the cycle the walk enters, or 0 if it ends at null.
class CycleLengthProblem : public ListQueryProblem {
public:
    using ListQueryProblem::ListQueryProblem;
    int answer(const std::vector<int>& input) const override;
};

// The last node before null on the walk; n when the list is empty or cyclic.
class LastNodeProblem : public ListQueryProblem {
public:
    using ListQueryProblem::ListQueryProblem;
    int answer(const std::vector<int>& input) const override;
};

}  // namespace algosearch
