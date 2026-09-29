#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/Random.hpp"
#include "problems/Problem.hpp"

namespace algosearch {

// Shared base for the section 39 post-freeze problems (palindrome,
// inversion_count, has_duplicate, is_sorted): the input is the whole array,
// output[0] is a scalar answer computed by answer(), and the rest of the
// array is unconstrained -- the same output convention as sum/min/max.
//
// Instances per level n: exhaustive over [0, 2] while that stays small
// (3^n <= 81), random arrays over [0, n] (a range where equal values -- and
// so duplicates and palindromes -- still occur), constructed arrays from
// constructedArrays() so a rare label (e.g. "is a palindrome") is not almost
// always 0, and the shared structural arrays (all-equal, duplicates,
// negatives, sorted, reversed, wide range). exhaustiveInputs() enumerates
// every array over [-1, 2] for n <= 6 for --benchmark's exhaustive check.
class ScalarFromArrayProblem : public Problem {
public:
    ScalarFromArrayProblem(int numVars, uint64_t seed);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override { return level + 1; }
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    std::vector<std::vector<int>> exhaustiveInputs(int n) const override;
    int numVars() const override { return numVars_; }

    // The correct output[0] for `values`.
    virtual int answer(const std::vector<int>& values) const = 0;

protected:
    // Arrays for which answer() takes its otherwise-rare value.
    virtual std::vector<std::vector<int>> constructedArrays(int n, Random& rng) const = 0;

private:
    Instance toInstance(std::vector<int> values) const;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
