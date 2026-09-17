#pragma once

#include "problems/Problem.hpp"

namespace algosearch {

// Classic bit-manipulation problem ("Single Number"): every value in the
// array appears an EVEN number of times except exactly one, which appears
// an odd number of times. Find it. Output convention matches the other
// decision/query problems: output[0] is the answer, rest of the array is
// unchecked.
//
// The textbook solution XORs every element together -- every value that
// appears an even number of times cancels itself out (x^x=0), leaving only
// the odd one out -- an O(n) single pass that's only reachable at all once
// the interpreter has a native XOR primitive (FINDINGS.md section 16's
// bitwise extension; genCountingLoopBlock's AccumKind::Xor).
//
// `level` is the number of PAIRS (not the raw array length): each instance
// has `level` paired values plus one unpaired value, so the array length is
// `2*level + 1`.
class SingleNumberProblem : public Problem {
public:
    explicit SingleNumberProblem(int exhaustiveMaxN = 4, int randomInstancesPerLevel = 40, int numVars = 4, uint64_t seed = 42);

    std::vector<Instance> generateInstances(int level) const override;
    std::string describeLevel(int level) const override;
    int nextLevel(int level) const override;
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    int numVars() const override { return numVars_; }

private:
    int exhaustiveMaxN_;
    int randomInstancesPerLevel_;
    int numVars_;
    uint64_t seed_;
};

}  // namespace algosearch
