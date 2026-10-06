#include "search/HoleGrammar.hpp"

#include <unordered_set>

namespace algosearch {

namespace {

void collectVars(const Expr& e, std::set<int>& out) {
    if (e.kind == ExprKind::Var) out.insert(e.varIndex);
    if (e.lhs) collectVars(*e.lhs, out);
    if (e.rhs) collectVars(*e.rhs, out);
}

std::vector<ExprPtr> atomsFor(const std::vector<int>& vars, bool withArray) {
    std::vector<ExprPtr> atoms;
    for (int v : vars) atoms.push_back(makeVar(v));
    for (int c = 0; c <= 2; ++c) atoms.push_back(makeIntConst(c));
    if (withArray) {
        for (int v : vars) atoms.push_back(makeArrayAccess(makeVar(v)));
        for (int c = 0; c <= 2; ++c) atoms.push_back(makeArrayAccess(makeIntConst(c)));
    }
    return atoms;
}

bool isAtom(const Expr& e) {
    if (e.kind == ExprKind::Var) return true;
    if (e.kind == ExprKind::IntConst) return e.intValue >= 0 && e.intValue <= 2;
    if (e.kind == ExprKind::ArrayAccess) return e.lhs->kind == ExprKind::Var || (e.lhs->kind == ExprKind::IntConst && e.lhs->intValue >= 0 && e.lhs->intValue <= 2);
    return false;
}

// Value of a hole expression on a traced state; `err` on an out-of-range
// read or a division by zero.
long evalState(const Expr& e, const std::vector<int>& vars, const std::vector<int>& arr, bool& err);

}  // namespace

void collectVars(const Stmt& s, std::set<int>& out) {
    if (s.kind == StmtKind::Assign) out.insert(s.varIndex);
    for (const Expr* e : {s.valueExpr.get(), s.indexExpr.get(), s.indexExpr2.get(), s.cond.get()}) {
        if (e) collectVars(*e, out);
    }
    for (const auto& c : s.body) collectVars(*c, out);
    for (const auto& c : s.elseBody) collectVars(*c, out);
}

bool inValueGrammar(const Expr& e) {
    if (isAtom(e)) return true;
    switch (e.kind) {
        case ExprKind::Add: case ExprKind::Sub: case ExprKind::Mul: case ExprKind::Div: case ExprKind::Mod:
            return isAtom(*e.lhs) && isAtom(*e.rhs);
        default: return false;
    }
}

bool inCondGrammar(const Expr& e) {
    switch (e.kind) {
        case ExprKind::Lt: case ExprKind::Gt: case ExprKind::Eq: case ExprKind::Le: case ExprKind::Ge: case ExprKind::Ne: break;
        default: return false;
    }
    if (!isAtom(*e.rhs)) return false;
    const Expr& l = *e.lhs;
    return isAtom(l) || ((l.kind == ExprKind::Add || l.kind == ExprKind::Sub) && isAtom(*l.lhs) && isAtom(*l.rhs));
}

ExprPtr randomValue(Random& rng, const std::vector<int>& vars) {
    std::vector<ExprPtr> atoms = atomsFor(vars, true);
    auto pick = [&] { return atoms[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(atoms.size()) - 1))]->clone(); };
    if (rng.chance(0.5)) return pick();
    const ExprKind ops[] = {ExprKind::Add, ExprKind::Sub, ExprKind::Mul, ExprKind::Div, ExprKind::Mod};
    return makeBinOp(ops[rng.uniformInt(0, 4)], pick(), pick());
}

ExprPtr randomCond(Random& rng, const std::vector<int>& vars) {
    std::vector<ExprPtr> atoms = atomsFor(vars, true);
    auto pick = [&] { return atoms[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(atoms.size()) - 1))]->clone(); };
    ExprPtr lhs = pick();
    if (rng.chance(0.5)) lhs = makeBinOp(rng.chance(0.5) ? ExprKind::Add : ExprKind::Sub, std::move(lhs), pick());
    const ExprKind ops[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    return makeCompare(ops[rng.uniformInt(0, 5)], std::move(lhs), pick());
}

namespace {

long evalState(const Expr& e, const std::vector<int>& vars, const std::vector<int>& arr, bool& err) {
    switch (e.kind) {
        case ExprKind::IntConst: return e.intValue;
        case ExprKind::Var: return vars[static_cast<size_t>(e.varIndex)];
        case ExprKind::ArrayAccess: {
            long i = evalState(*e.lhs, vars, arr, err);
            if (err || i < 0 || i >= static_cast<long>(arr.size())) {
                err = true;
                return 0;
            }
            return arr[static_cast<size_t>(i)];
        }
        default: break;
    }
    long a = evalState(*e.lhs, vars, arr, err), b = evalState(*e.rhs, vars, arr, err);
    if (err) return 0;
    switch (e.kind) {
        case ExprKind::Add: return static_cast<int>(a + b);
        case ExprKind::Sub: return static_cast<int>(a - b);
        case ExprKind::Mul: return static_cast<int>(a * b);
        case ExprKind::Div:
        case ExprKind::Mod:
            if (b == 0) {
                err = true;
                return 0;
            }
            return e.kind == ExprKind::Div ? a / b : a % b;
        case ExprKind::Lt: return a < b;
        case ExprKind::Gt: return a > b;
        case ExprKind::Eq: return a == b;
        case ExprKind::Le: return a <= b;
        case ExprKind::Ge: return a >= b;
        case ExprKind::Ne: return a != b;
        default: err = true; return 0;
    }
}

}  // namespace

// Visits every hole slot of `s` in a fixed order; `fn(stmt, slot, isCond)`.
void forEachSlot(Stmt& s, const std::function<void(Stmt&, ExprPtr&, bool)>& fn) {
    switch (s.kind) {
        case StmtKind::Assign:
        case StmtKind::AssignArray:
            fn(s, s.valueExpr, false);
            break;
        case StmtKind::If:
        case StmtKind::While:
            fn(s, s.cond, true);
            break;
        default:
            break;
    }
    for (auto& c : s.body) forEachSlot(*c, fn);
    for (auto& c : s.elseBody) forEachSlot(*c, fn);
}

// Clone of `program` with site k replaced by `fill`; `siteStmt` receives the
// statement holding the hole (for tracing).
Program withHole(const Stmt& program, size_t k, const Expr* fill, const Stmt** siteStmt) {
    Program copy = program.clone();
    size_t i = 0;
    forEachSlot(*copy, [&](Stmt& st, ExprPtr& slot, bool) {
        if (i++ != k) return;
        if (fill) slot = fill->clone();
        if (siteStmt) *siteStmt = &st;
    });
    return copy;
}




std::vector<ExprPtr> enumerateValues(const std::vector<int>& vars) {
    std::vector<ExprPtr> atoms = atomsFor(vars, true), out;
    for (const auto& a : atoms) out.push_back(a->clone());
    const ExprKind ops[] = {ExprKind::Add, ExprKind::Sub, ExprKind::Mul, ExprKind::Div, ExprKind::Mod};
    for (const auto& a : atoms) {
        for (const auto& b : atoms) {
            if (a->kind == ExprKind::IntConst && b->kind == ExprKind::IntConst) continue;
            for (ExprKind op : ops) out.push_back(makeBinOp(op, a->clone(), b->clone()));
        }
    }
    return out;
}

std::vector<ExprPtr> enumerateConds(const std::vector<int>& vars) {
    std::vector<ExprPtr> atoms = atomsFor(vars, true), terms, out;
    for (const auto& a : atoms) terms.push_back(a->clone());
    for (const auto& a : atoms) {
        for (const auto& b : atoms) {
            if (a->kind == ExprKind::IntConst && b->kind == ExprKind::IntConst) continue;
            terms.push_back(makeBinOp(ExprKind::Add, a->clone(), b->clone()));
            terms.push_back(makeBinOp(ExprKind::Sub, a->clone(), b->clone()));
        }
    }
    const ExprKind ops[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    for (const auto& l : terms) {
        for (const auto& r : atoms) {
            for (ExprKind op : ops) out.push_back(makeCompare(op, l->clone(), r->clone()));
        }
    }
    return out;
}


std::vector<ExprPtr> dedupeOnStates(std::vector<ExprPtr> exprs, const std::vector<HoleState>& states) {
    std::unordered_set<std::string> seen;
    std::vector<ExprPtr> reps;
    for (auto& e : exprs) {
        std::string key;
        for (const auto& st : states) {
            bool err = false;
            long v = evalState(*e, st.vars, st.arr, err);
            key += err ? "E," : std::to_string(v) + ",";
        }
        if (seen.insert(key).second) reps.push_back(std::move(e));
    }
    return reps;
}

}  // namespace algosearch
