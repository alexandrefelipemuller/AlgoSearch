#pragma once

#include <memory>
#include <string>
#include <vector>

#include "ast/Expr.hpp"

namespace algosearch {

// Explicit kinds for every statement form supported by Mini-C.
enum class StmtKind {
    Assign,       // var[varIndex] = valueExpr
    AssignArray,  // arr[indexExpr] = valueExpr
    If,           // if (cond) { body } else { elseBody }
    While,        // while (cond) { body }
    Swap,         // swap(arr[indexExpr], arr[indexExpr2])
    Return,       // return (ends execution; final array state is the output)
    Block,        // sequence of statements (body)
    // Second-array extension counterpart to AssignArray (see
    // ExprKind::AuxAccess in ast/Expr.hpp) -- opt-in via
    // GenConfig::secondArrayBias, zero probability reproduces prior
    // behavior exactly.
    AssignAux,    // aux[indexExpr] = valueExpr
    // Recursion extension, opt-in via GenConfig::recursionCallBias (0.0 =
    // never generated, no behavior change for any existing problem). A
    // self-call: re-invokes the whole program from its own start, on a
    // FRESH set of scalar variables (v0 reset to n as always, v1 seeded
    // from indexExpr's value, v2 seeded from indexExpr2's value -- the
    // reserved "lo"/"hi" convention documented in Interpreter.cpp), sharing
    // arr/aux with the caller. The caller's own variables are restored
    // unchanged once the call returns -- ordinary call-by-value semantics,
    // not a shared/global frame. Depth is capped (Interpreter's
    // maxRecursionDepth) independently of stepLimit: unbounded self-
    // recursion would blow the C++ call stack long before a large
    // stepLimit is reached, unlike an unbounded while loop.
    Call,         // call(indexExpr /*lo*/, indexExpr2 /*hi*/)
    // Control-flow extension (see GenConfig::enableControlFlowExt): the
    // missing primitive an exact/pruning algorithm (branch-and-bound-style
    // early exit) needs and Mini-C didn't have -- every other statement
    // kind runs unconditionally to completion, so nothing could express
    // "abandon the rest of this loop iteration/this loop entirely." Valid
    // only inside a While's body (possibly nested through If); the
    // generator only ever emits them there (see genStmt's `insideLoop`
    // threading), and the interpreter treats one that escapes past every
    // enclosing loop as a runtime error rather than silently completing.
    Continue,     // skip to the nearest enclosing while's condition re-check
    Break         // exit the nearest enclosing while loop entirely
};

// A single statement AST node. Fields are only meaningful for the relevant kind:
//  - Assign: varIndex, valueExpr
//  - AssignArray: indexExpr, valueExpr
//  - If: cond, body (then-branch), elseBody (optional else-branch)
//  - While: cond, body
//  - Swap: indexExpr, indexExpr2
//  - Return: (no extra fields)
//  - Block: body (the statements it contains)
struct Stmt {
    StmtKind kind;
    int varIndex = 0;
    ExprPtr valueExpr;
    ExprPtr indexExpr;
    ExprPtr indexExpr2;
    ExprPtr cond;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> elseBody;

    std::unique_ptr<Stmt> clone() const;
    int size() const;  // number of AST nodes, statements + expressions
    std::string toString(int indent = 0) const;
};

using StmtPtr = std::unique_ptr<Stmt>;
using Program = StmtPtr;  // root is always a Block

StmtPtr makeBlock();
StmtPtr makeAssign(int varIndex, ExprPtr value);
StmtPtr makeAssignArray(ExprPtr index, ExprPtr value);
StmtPtr makeAssignAux(ExprPtr index, ExprPtr value);
StmtPtr makeCall(ExprPtr loExpr, ExprPtr hiExpr);
StmtPtr makeIf(ExprPtr cond, std::vector<StmtPtr> body, std::vector<StmtPtr> elseBody = {});
StmtPtr makeWhile(ExprPtr cond, std::vector<StmtPtr> body);
StmtPtr makeSwap(ExprPtr index1, ExprPtr index2);
StmtPtr makeReturn();
StmtPtr makeContinue();
StmtPtr makeBreak();

// Pretty-prints `program` after three cosmetic-only normalizations:
//  1. Folds `if`/`while` conditions that are provably always-false (or,
//     for `if`, always-true) from their own text alone -- two integer
//     literals, or the same expression compared with itself (`v6 < v6`,
//     `0 >= 3`) -- splicing in the branch that always runs and dropping a
//     never-executing `while` entirely. Never touches a condition whose
//     truth depends on what a variable's value actually is at runtime; see
//     the doc comment on foldConstantConditions in Stmt.cpp.
//  2. Drops every `Assign` (anywhere, any nesting depth) whose target
//     variable is not LIVE at that point -- i.e. not read again before
//     either the next write to it or the program ending, on any reachable
//     control-flow path (full backward liveness with a fixed point for
//     loops; see the analysis in Stmt.cpp above canonicalText, and
//     FINDINGS.md section 30 for why a cruder "read anywhere in the whole
//     program" check was too weak: it missed a redundant LAST write to an
//     otherwise-live variable, e.g. one more write to a loop counter right
//     after the loop it drove already ended).
//  3. Renumbers every remaining scratch variable (v1, v2, ... -- v0 is left
//     untouched, since it is always the implicit n/array-length register,
//     never an arbitrary choice) to the order it is first encountered in a
//     depth-first, print-order walk.
// Two programs that differ ONLY in dead stores and/or which scratch-
// variable NUMBER they happened to pick for the same role produce
// IDENTICAL canonical text; anything that changes actual behavior (control
// flow shape, literals, which role does what, a live store) still differs.
//
// Exists so callers that need "is this the SAME program, ignoring cosmetic
// differences" (program-id hashing, the beam search's divergence-window
// dedup) can hash/compare this instead of the raw pretty-printed text. See
// FINDINGS.md sections 29-30: hashing raw text let the archive count the
// same selection-sort skeleton, re-numbered and/or padded with a
// differently-sized tail of dead stores by mutation, as several "distinct"
// solutions -- inflating diversity counts and letting cosmetic
// rediscoveries look like new progress to the divergence window. Never
// used on the actual program the search keeps/executes -- only on a
// throwaway clone built for this comparison.
std::string canonicalText(const Stmt& program);

}  // namespace algosearch
