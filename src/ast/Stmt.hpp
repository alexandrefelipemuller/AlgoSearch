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
    Block         // sequence of statements (body)
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
StmtPtr makeIf(ExprPtr cond, std::vector<StmtPtr> body, std::vector<StmtPtr> elseBody = {});
StmtPtr makeWhile(ExprPtr cond, std::vector<StmtPtr> body);
StmtPtr makeSwap(ExprPtr index1, ExprPtr index2);
StmtPtr makeReturn();

}  // namespace algosearch
