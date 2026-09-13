#pragma once

#include <string>

namespace algosearch {

struct Expr;
struct Stmt;

// Renders Mini-C source text for an expression / statement subtree.
// This doubles as the canonical serialization used for structural
// AST deduplication (see search/Dedup.hpp).
std::string printExpr(const Expr& e);
std::string printStmt(const Stmt& s, int indent = 0);

}  // namespace algosearch
