#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 120 open-specification problem `any_duplicate`: output[0] = ANY value that occurs at least twice, or -1
// when all values are distinct (inputs are non-negative in the training levels; the adversarial ones shift them, so
// -1 is read as "no duplicate" only when the array really has none). Several values can qualify; the checker accepts
// each of them, so nested pair scans, sort-and-scan and counting variants are all correct.
class AnyDuplicateProblem : public ScalarFromArrayProblem {
public:
    explicit AnyDuplicateProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
