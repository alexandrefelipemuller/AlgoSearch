#pragma once

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "core/Random.hpp"

namespace algosearch {

// Section 65's hole grammar, shared by the skeleton ablation (section 65)
// and the search's enumerative repair (section 66). Atoms are the given
// scalars, the literals 0..2, arr[v] for each scalar v, and arr[0..2].
// Value holes: an atom, or atom op atom with op in + - * / %. Condition
// holes: an atom or atom +/- atom, compared (< > == <= >= !=) with an atom.

// Visits every hole slot of `s` in a fixed order: the value of every
// scalar/array assignment and the condition of every If/While.
void forEachSlot(Stmt& s, const std::function<void(Stmt&, ExprPtr&, bool isCond)>& fn);

// Clone of `program` with slot k replaced by `fill` (left as is when null);
// `siteStmt` receives the statement holding slot k, for tracing.
Program withHole(const Stmt& program, size_t k, const Expr* fill, const Stmt** siteStmt = nullptr);

// Scalars read or assigned anywhere in `s`.
void collectVars(const Stmt& s, std::set<int>& out);

std::vector<ExprPtr> enumerateValues(const std::vector<int>& vars);
std::vector<ExprPtr> enumerateConds(const std::vector<int>& vars);

// Is `e` expressible in the value / condition grammar (over any scalars)?
bool inValueGrammar(const Expr& e);
bool inCondGrammar(const Expr& e);

// A uniformly drawn grammar expression of each kind.
ExprPtr randomValue(Random& rng, const std::vector<int>& vars);
ExprPtr randomCond(Random& rng, const std::vector<int>& vars);

struct HoleState {
    std::vector<int> vars, arr;
};

// One representative per class of `exprs` with identical values (or
// errors: out-of-range read, division by zero) on every state.
std::vector<ExprPtr> dedupeOnStates(std::vector<ExprPtr> exprs, const std::vector<HoleState>& states);

}  // namespace algosearch
