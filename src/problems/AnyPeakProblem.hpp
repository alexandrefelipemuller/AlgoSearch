#pragma once

#include "problems/ScalarFromArrayProblem.hpp"

namespace algosearch {

// Section 120 open-specification problem `any_peak`: output[0] = ANY index whose element is >= each existing
// neighbour. isSolution() is a property check, not a comparison with one reference answer, so the first peak, the
// argmax and a binary-search peak are all correct. answer() is only the reference label stored in instances.
class AnyPeakProblem : public ScalarFromArrayProblem {
public:
    explicit AnyPeakProblem(int numVars = 8, uint64_t seed = 42) : ScalarFromArrayProblem(numVars, seed) {}
    int answer(const std::vector<int>& values) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;

protected:
    std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const override;
};

}  // namespace algosearch
