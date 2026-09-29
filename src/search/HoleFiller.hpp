#pragma once

#include <vector>

#include "ast/Expr.hpp"

namespace algosearch {

// Idea A (FINDINGS.md section 41): fill a condition "hole" in a hand-given
// program shape by EXHAUSTIVE enumeration instead of mutation. Section 18
// found that `cost < best` in TSP branch-and-bound sits at an isolated point
// of expression space that no mutation-based search climbs to (no gradient:
// every non-pruning tautology scores the same). Enumeration needs no
// gradient: generate every small condition, collapse the ones that behave
// identically on recorded program states (observational equivalence, the
// standard pruning of enumerative synthesis), evaluate one representative
// per class for real, and keep the fastest correct one.

// Every comparison `lhs op rhs` with lhs a term (an atom, or atom +/- atom)
// and rhs an atom, where atoms are the given variables and the literals
// 0..maxConst, and op is one of < > == <= >= !=.
std::vector<ExprPtr> enumerateConditions(const std::vector<int>& vars, int maxConst);

// Value of a Var/IntConst/Add/Sub/comparison expression on a snapshot of the
// scalar variables (no array access -- enumerated candidates never read arr).
int evalOnSnapshot(const Expr& e, const std::vector<int>& vars);

// One representative per equivalence class of `conditions`, where two
// conditions are equivalent if they have the same truth value on every
// snapshot. Keeps the first (smallest-index) member of each class.
std::vector<ExprPtr> dedupeByTruthVector(std::vector<ExprPtr> conditions, const std::vector<std::vector<int>>& snapshots);

// Soundness oracle for the TSP branch-and-bound guard (section 41, labels
// run). Given the scalars `vars` and the array `arr` at one evaluation of the
// guard -- in buildTspBranchAndBoundWithGuard's layout and variable order
// (nv, size, uBase, chBase, pcBase, mBase, best, level, c, w, cost at
// vars[1..11]) -- returns true iff pruning here is SAFE: no tour completing
// the partial path path[0..level-1] -> c costs strictly less than `best`.
// Pruning a subtree that holds a strictly better tour is the unsound kind;
// this labels each evaluation using nothing but the instance's own weights
// (exact brute force over the unvisited vertices, fine for V <= ~9).
bool tspPruneIsSafe(const std::vector<int>& vars, const std::vector<int>& arr);

}  // namespace algosearch
