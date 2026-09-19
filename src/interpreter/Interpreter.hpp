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
    // maxRecursionDepth guards the recursion extension's Call statement
    // (see StmtKind::Call): a hard cap on self-call nesting, independent of
    // stepLimit, because unbounded recursion overflows the real C++ call
    // stack long before a generous stepLimit would flag it as a timeout.
    // Irrelevant (never checked) for any program that never emits Call.
    // seedRecursionBounds: when true, the initial (non-recursive) call also
    // seeds v1=0/v2=n -- the same "lo"/"hi" reserved-variable convention a
    // recursive Call uses (see StmtKind::Call) -- so a divide-and-conquer
    // skeleton's top-level entry is self-similar with every recursive
    // frame instead of needing special-cased code for "am I the outermost
    // call". False (default) reproduces prior behavior exactly: v1/v2 stay
    // ordinary zero-initialized scratch scalars, unchanged for every
    // problem/run that doesn't opt into the recursion extension.
    explicit Interpreter(long stepLimit, long maxRecursionDepth = 200, bool seedRecursionBounds = false)
        : stepLimit_(stepLimit), maxRecursionDepth_(maxRecursionDepth), seedRecursionBounds_(seedRecursionBounds) {}

    // Runs `program` (a Block) with `input` loaded into the array and
    // `numVars` scalar variables (all initialized to 0, except var 0 which
    // is initialized to the array length). On COMPLETED, `output` holds the
    // final array contents. `stepsOut`, when non-null, receives the number
    // of interpreter steps the run actually consumed -- optional (default
    // null, zero cost/behavior change for every existing caller) so a
    // caller that wants a speed signal (see SearchConfig::speedBonusWeight)
    // can have one without every other call site needing to change.
    RunStatus run(const Stmt& program, const std::vector<int>& input, int numVars, std::vector<int>& output,
                  long* stepsOut = nullptr) const;

private:
    long stepLimit_;
    long maxRecursionDepth_;
    bool seedRecursionBounds_;
};

}  // namespace algosearch
