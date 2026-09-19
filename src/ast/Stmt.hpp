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

}  // namespace algosearch
