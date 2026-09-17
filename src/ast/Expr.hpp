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
    Eq,           // lhs == rhs
    Le,           // lhs <= rhs
    Ge,           // lhs >= rhs
    Ne,           // lhs != rhs
    // Arithmetic extension (FINDINGS.md section 13's "language ceiling"):
    // native O(1) multiply/divide/modulo, gated OFF by default in the
    // generator (GenConfig::enableArithmeticExt) so every existing problem's
    // search space/probabilities are unchanged unless a problem opts in.
    Mul,          // lhs * rhs
    Div,          // lhs / rhs (truncating; rhs == 0 is a runtime error)
    Mod,          // lhs % rhs (C++ semantics; rhs == 0 is a runtime error)
    // Bit-manipulation extension, same opt-in gating
    // (GenConfig::enableBitwiseExt).
    BitAnd,       // lhs & rhs
    BitOr,        // lhs | rhs
    BitXor,       // lhs ^ rhs
    Shl,          // lhs << rhs (rhs must be in [0,31], else a runtime error)
    Shr           // lhs >> rhs (arithmetic shift; rhs must be in [0,31])
};

// A single expression AST node. Fields are only meaningful for the relevant kind:
//  - IntConst: intValue
//  - Var:      varIndex
//  - ArrayAccess: lhs = index expression
//  - Add/Sub/Lt/Gt/Eq/Le/Ge/Ne/Mul/Div/Mod/BitAnd/BitOr/BitXor/Shl/Shr: lhs, rhs
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
ExprPtr makeCompare(ExprKind kind, ExprPtr lhs, ExprPtr rhs);  // Lt/Gt/Eq/Le/Ge/Ne

bool isBinaryKind(ExprKind kind);

}  // namespace algosearch
