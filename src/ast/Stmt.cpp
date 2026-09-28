#include "ast/Stmt.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "language/PrettyPrinter.hpp"

namespace algosearch {

namespace {

// -- Constant-condition folding for canonicalText() --
//
// A generated `if`/`while` can end up with a condition that is provably
// always false (`0 >= 3`, `v6 < v6`) purely from its own text, with no need
// to track any variable's runtime value. That branch/loop body then never
// executes, but everything it reads still looks "live" to the backward-
// liveness pass above (it only tracks whether a read COULD occur, not
// whether the code containing it can ever run) -- so growth padding shaped
// like `if (0 >= 3) { ...uses several vars... }` survived the liveness fix
// (FINDINGS.md section 30's remaining gap, per the follow-up run). Folding
// this out FIRST lets the liveness pass then also strip whatever was only
// "read" inside the now-removed branch.
//
// Deliberately narrow and conservative: only decides a condition's truth
// value from (a) two integer literals, evaluated directly, or (b) both
// sides being the exact same expression, using the algebraic identities
// that hold for EVERY integer (`x < x`/`x > x`/`x != x` always false,
// `x <= x`/`x >= x`/`x == x` always true) -- never anything that requires
// knowing what a variable's value actually is at runtime (e.g. the
// `if (v7 <= v4) {...}` shape from the same run, where v4 happens to
// always be 0 and v7 happens to always end at v0, is NOT decidable this
// way and is correctly left untouched; that needs real constant
// propagation, not condition folding). Only ever removes a branch that
// provably cannot run under ANY execution, on ANY input -- never one whose
// reachability depends on a specific variable's value. The decision itself
// (exprEqual/constantConditionValue) lives in ast/Expr.hpp, shared with the
// generator's GenConfig::pruneGeneration.

// Splices a constant-false `if`'s elseBody (or a constant-true `if`'s
// body) in place of the `if` itself, and drops a constant-false `while`
// entirely (zero iterations). Recurses bottom-up so a fold inside a kept
// branch happens before that branch's own condition (if any) is checked.
void foldConstantConditions(Stmt& s);

void foldConstantConditions(std::vector<StmtPtr>& stmts) {
    std::vector<StmtPtr> result;
    result.reserve(stmts.size());
    for (auto& s : stmts) {
        foldConstantConditions(*s);
        if (s->kind == StmtKind::If && s->cond) {
            if (auto val = constantConditionValue(*s->cond)) {
                auto& chosen = *val ? s->body : s->elseBody;
                for (auto& c : chosen) result.push_back(std::move(c));
                continue;
            }
        }
        if (s->kind == StmtKind::While && s->cond) {
            if (auto val = constantConditionValue(*s->cond); val && !*val) continue;
        }
        result.push_back(std::move(s));
    }
    stmts = std::move(result);
}

void foldConstantConditions(Stmt& s) {
    foldConstantConditions(s.body);
    foldConstantConditions(s.elseBody);
}

void collectAndRemapVar(int& varIndex, std::unordered_map<int, int>& canon, int& next) {
    if (varIndex == 0) return;
    auto it = canon.find(varIndex);
    if (it == canon.end()) it = canon.emplace(varIndex, next++).first;
    varIndex = it->second;
}

void canonicalizeVars(Expr& e, std::unordered_map<int, int>& canon, int& next) {
    if (e.kind == ExprKind::Var) collectAndRemapVar(e.varIndex, canon, next);
    if (e.lhs) canonicalizeVars(*e.lhs, canon, next);
    if (e.rhs) canonicalizeVars(*e.rhs, canon, next);
}

void canonicalizeVars(Stmt& s, std::unordered_map<int, int>& canon, int& next) {
    if (s.kind == StmtKind::Assign) collectAndRemapVar(s.varIndex, canon, next);
    if (s.cond) canonicalizeVars(*s.cond, canon, next);
    if (s.indexExpr) canonicalizeVars(*s.indexExpr, canon, next);
    if (s.indexExpr2) canonicalizeVars(*s.indexExpr2, canon, next);
    if (s.valueExpr) canonicalizeVars(*s.valueExpr, canon, next);
    for (auto& c : s.body) canonicalizeVars(*c, canon, next);
    for (auto& c : s.elseBody) canonicalizeVars(*c, canon, next);
}

// -- Dead-store elimination for canonicalText(), via real backward liveness --
//
// The first version of this pass (FINDINGS.md section 29) only stripped an
// Assign if its target was never read ANYWHERE in the whole program --
// sound, but too weak: the dominant real pattern turned out to be a
// redundant LAST write to a variable that IS used earlier (e.g. the loop
// counter, one more time, after the loop it was driving already ended),
// which that whole-program check can't see (section 30). This version
// tracks, at every point in the program, exactly which variables might
// still be read before the program ends -- standard backward liveness --
// and strips an Assign only when its target is not in that set. A variable
// is live entering a statement list if some read of it could still occur
// before the next write to it or the program's end, considering every
// control-flow path (both branches of an `if`; a `while`, handled with a
// small fixed-point iteration since a loop's body can feed its own next
// iteration through the back-edge).
//
// Soundness (never strips something that might actually be read) matters
// far more than precision here: this is used only to decide whether two
// programs count as "the same" for dedup (program-id hashing, the
// divergence window's archive) -- wrongly merging two behaviorally
// DIFFERENT programs would hide a real result, so every merge below is
// conservative by construction. `Return`/`Continue`/`Break` are treated as
// passing the live set through unchanged (sound: this never claims a
// variable is dead when it might not be, it just occasionally misses a
// strip opportunity around them, which is fine). Never runs on the actual
// program kept/executed by the search -- only on the throwaway clone
// canonicalText() hashes/compares.
using VarSet = std::unordered_set<int>;

void addUses(const Expr* e, VarSet& uses) {
    if (!e) return;
    if (e->kind == ExprKind::Var) uses.insert(e->varIndex);
    addUses(e->lhs.get(), uses);
    addUses(e->rhs.get(), uses);
}

// Variables this statement itself reads (its own cond/indexExpr/indexExpr2/
// valueExpr) -- NOT including nested body/elseBody, which callers handle
// recursively via the liveness equations below.
VarSet usesOf(const Stmt& s) {
    VarSet uses;
    addUses(s.cond.get(), uses);
    addUses(s.indexExpr.get(), uses);
    addUses(s.indexExpr2.get(), uses);
    addUses(s.valueExpr.get(), uses);
    return uses;
}

VarSet unioned(VarSet a, const VarSet& b) {
    a.insert(b.begin(), b.end());
    return a;
}

// Pure (non-mutating): the live-in set entering this statement list, given
// what's live after it. Used only to find a While loop's fixed point
// BEFORE actually stripping anything from its body (see below) -- keeping
// this side-effect-free means the fixed-point search can't itself be
// destabilized by partial mutation.
VarSet computeLiveIn(const Stmt& s, const VarSet& liveOut);

VarSet computeLiveIn(const std::vector<StmtPtr>& stmts, VarSet liveOut) {
    for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) liveOut = computeLiveIn(**it, liveOut);
    return liveOut;
}

VarSet computeLiveIn(const Stmt& s, const VarSet& liveOut) {
    switch (s.kind) {
        case StmtKind::Assign: {
            VarSet in = liveOut;
            in.erase(s.varIndex);
            return unioned(in, usesOf(s));
        }
        case StmtKind::AssignArray:
        case StmtKind::AssignAux:
        case StmtKind::Swap:
        case StmtKind::Call:
            return unioned(liveOut, usesOf(s));
        case StmtKind::Return:
        case StmtKind::Continue:
        case StmtKind::Break:
            return liveOut;
        case StmtKind::Block:
            return computeLiveIn(s.body, liveOut);
        case StmtKind::If: {
            VarSet in = unioned(computeLiveIn(s.body, liveOut), computeLiveIn(s.elseBody, liveOut));
            return unioned(in, usesOf(s));
        }
        case StmtKind::While: {
            VarSet condUses = usesOf(s);
            VarSet L = unioned(liveOut, condUses);
            VarSet prev;
            do {
                prev = L;
                L = unioned(unioned(liveOut, condUses), computeLiveIn(s.body, L));
            } while (L != prev);
            return L;
        }
    }
    return liveOut;
}

// Mutating counterpart: same equations, but actually erases a dead Assign
// (and recurses into If/While bodies, erasing within them too) instead of
// just computing the live-in set. For a While, the body is stripped using
// the ALREADY-CONVERGED fixed point from computeLiveIn above -- mutation
// never runs during the fixed-point search itself.
VarSet stripDeadAndComputeLiveIn(Stmt& s, const VarSet& liveOut);

VarSet stripDeadAndComputeLiveIn(std::vector<StmtPtr>& stmts, const VarSet& liveOut) {
    VarSet live = liveOut;
    for (size_t i = stmts.size(); i-- > 0;) {
        Stmt& s = *stmts[i];
        if (s.kind == StmtKind::Assign && live.count(s.varIndex) == 0) {
            stmts.erase(stmts.begin() + static_cast<long>(i));
            continue;  // removing a dead assign adds no new uses; live unchanged
        }
        live = stripDeadAndComputeLiveIn(s, live);
    }
    return live;
}

VarSet stripDeadAndComputeLiveIn(Stmt& s, const VarSet& liveOut) {
    switch (s.kind) {
        case StmtKind::Assign: {
            VarSet in = liveOut;
            in.erase(s.varIndex);
            return unioned(in, usesOf(s));
        }
        case StmtKind::AssignArray:
        case StmtKind::AssignAux:
        case StmtKind::Swap:
        case StmtKind::Call:
            return unioned(liveOut, usesOf(s));
        case StmtKind::Return:
        case StmtKind::Continue:
        case StmtKind::Break:
            return liveOut;
        case StmtKind::Block:
            return stripDeadAndComputeLiveIn(s.body, liveOut);
        case StmtKind::If: {
            VarSet in =
                unioned(stripDeadAndComputeLiveIn(s.body, liveOut), stripDeadAndComputeLiveIn(s.elseBody, liveOut));
            return unioned(in, usesOf(s));
        }
        case StmtKind::While: {
            VarSet condUses = usesOf(s);
            VarSet L = unioned(liveOut, condUses);
            VarSet prev;
            do {
                prev = L;
                L = unioned(unioned(liveOut, condUses), computeLiveIn(s.body, L));
            } while (L != prev);
            VarSet inBody = stripDeadAndComputeLiveIn(s.body, L);
            return unioned(unioned(liveOut, condUses), inBody);
        }
    }
    return liveOut;
}

}  // namespace

std::unique_ptr<Stmt> Stmt::clone() const {
    auto n = std::make_unique<Stmt>();
    n->kind = kind;
    n->varIndex = varIndex;
    if (valueExpr) n->valueExpr = valueExpr->clone();
    if (indexExpr) n->indexExpr = indexExpr->clone();
    if (indexExpr2) n->indexExpr2 = indexExpr2->clone();
    if (cond) n->cond = cond->clone();
    n->body.reserve(body.size());
    for (const auto& s : body) n->body.push_back(s->clone());
    n->elseBody.reserve(elseBody.size());
    for (const auto& s : elseBody) n->elseBody.push_back(s->clone());
    return n;
}

int Stmt::size() const {
    int total = 1;
    if (valueExpr) total += valueExpr->size();
    if (indexExpr) total += indexExpr->size();
    if (indexExpr2) total += indexExpr2->size();
    if (cond) total += cond->size();
    for (const auto& s : body) total += s->size();
    for (const auto& s : elseBody) total += s->size();
    return total;
}

std::string Stmt::toString(int indent) const { return printStmt(*this, indent); }

StmtPtr makeBlock() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Block;
    return n;
}

StmtPtr makeAssign(int varIndex, ExprPtr value) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Assign;
    n->varIndex = varIndex;
    n->valueExpr = std::move(value);
    return n;
}

StmtPtr makeAssignArray(ExprPtr index, ExprPtr value) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::AssignArray;
    n->indexExpr = std::move(index);
    n->valueExpr = std::move(value);
    return n;
}

StmtPtr makeAssignAux(ExprPtr index, ExprPtr value) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::AssignAux;
    n->indexExpr = std::move(index);
    n->valueExpr = std::move(value);
    return n;
}

StmtPtr makeCall(ExprPtr loExpr, ExprPtr hiExpr) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Call;
    n->indexExpr = std::move(loExpr);
    n->indexExpr2 = std::move(hiExpr);
    return n;
}

StmtPtr makeIf(ExprPtr cond, std::vector<StmtPtr> body, std::vector<StmtPtr> elseBody) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::If;
    n->cond = std::move(cond);
    n->body = std::move(body);
    n->elseBody = std::move(elseBody);
    return n;
}

StmtPtr makeWhile(ExprPtr cond, std::vector<StmtPtr> body) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::While;
    n->cond = std::move(cond);
    n->body = std::move(body);
    return n;
}

StmtPtr makeSwap(ExprPtr index1, ExprPtr index2) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Swap;
    n->indexExpr = std::move(index1);
    n->indexExpr2 = std::move(index2);
    return n;
}

StmtPtr makeReturn() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Return;
    return n;
}

StmtPtr makeContinue() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Continue;
    return n;
}

StmtPtr makeBreak() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Break;
    return n;
}

std::string canonicalText(const Stmt& program) {
    StmtPtr clone = program.clone();

    // Fold provably-unreachable branches FIRST, so the liveness pass below
    // sees whatever they used to "read" as gone too.
    foldConstantConditions(*clone);

    // Nothing is live after the whole program ends -- only the final
    // arr/aux state is ever observed (Problem::isSolution), never a scalar
    // variable's value.
    stripDeadAndComputeLiveIn(*clone, VarSet{});

    std::unordered_map<int, int> canon;
    int next = 1;
    canonicalizeVars(*clone, canon, next);
    return clone->toString();
}

}  // namespace algosearch
