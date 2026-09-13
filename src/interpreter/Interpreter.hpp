#pragma once

#include <vector>

#include "ast/Stmt.hpp"
#include "core/Types.hpp"

namespace algosearch {

// Small deterministic virtual machine that executes a Mini-C AST.
//
// Execution state is a fixed number of scalar integer variables plus a
// single integer array (the structured input/output). A step counter is
// incremented for every statement/expression evaluated; exceeding
// `stepLimit` deterministically aborts the run as TIMEOUT instead of
// relying on wall-clock time.
class Interpreter {
public:
    explicit Interpreter(long stepLimit) : stepLimit_(stepLimit) {}

    // Runs `program` (a Block) with `input` loaded into the array and
    // `numVars` scalar variables (all initialized to 0, except var 0 which
    // is initialized to the array length). On COMPLETED, `output` holds the
    // final array contents.
    RunStatus run(const Stmt& program, const std::vector<int>& input, int numVars, std::vector<int>& output) const;

private:
    long stepLimit_;
};

}  // namespace algosearch
