#pragma once

#include <memory>
#include <string>

namespace algosearch {

// Explicit kinds for every expression form supported by Mini-C.
enum class ExprKind {
    IntConst,     // literal integer
    Var,          // scalar variable read
    ArrayAccess,  // arr[index]
    Add,          // lhs + rhs
    Sub,          // lhs - rhs
    Lt,           // lhs < rhs
    Gt,           // lhs > rhs
    Eq            // lhs == rhs
};

// A single expression AST node. Fields are only meaningful for the relevant kind:
//  - IntConst: intValue
//  - Var:      varIndex
//  - ArrayAccess: lhs = index expression
//  - Add/Sub/Lt/Gt/Eq: lhs, rhs
struct Expr {
    ExprKind kind;
    int intValue = 0;
    int varIndex = 0;
    std::unique_ptr<Expr> lhs;
    std::unique_ptr<Expr> rhs;

    std::unique_ptr<Expr> clone() const;
    int size() const;  // number of nodes in this subtree
    std::string toString() const;
};

using ExprPtr = std::unique_ptr<Expr>;

ExprPtr makeIntConst(int value);
ExprPtr makeVar(int varIndex);
ExprPtr makeArrayAccess(ExprPtr index);
ExprPtr makeBinOp(ExprKind kind, ExprPtr lhs, ExprPtr rhs);  // Add/Sub
ExprPtr makeCompare(ExprKind kind, ExprPtr lhs, ExprPtr rhs);  // Lt/Gt/Eq

bool isBinaryKind(ExprKind kind);

}  // namespace algosearch
