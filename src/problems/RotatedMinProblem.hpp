#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 123: the minimum of a rotated strictly increasing array (distinct values). Every instance is a rotation, so
// a halving search is correct while it would fail on arbitrary arrays; output[0] = the minimum value.
class RotatedMinProblem : public ScalarFromArrayProblem {
public:
    explicit RotatedMinProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;
    std::vector<Instance> generateInstances(int level) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    std::vector<std::vector<int>> exhaustiveInputs(int) const override { return {}; }

protected:
    std::vector<std::vector<int>> constructedArrays(int, Random&) const override { return {}; }
};

}  // namespace algosearch
