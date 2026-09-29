#include "search/HoleFiller.hpp"

#include <string>
#include <unordered_set>

namespace algosearch {

std::vector<ExprPtr> enumerateConditions(const std::vector<int>& vars, int maxConst) {
    std::vector<ExprPtr> atoms;
    for (int v : vars) atoms.push_back(makeVar(v));
    for (int c = 0; c <= maxConst; ++c) atoms.push_back(makeIntConst(c));

    std::vector<ExprPtr> terms;
    for (const auto& a : atoms) terms.push_back(a->clone());
    for (const auto& a : atoms) {
        for (const auto& b : atoms) {
            if (a->kind == ExprKind::IntConst && b->kind == ExprKind::IntConst) continue;  // constant, already covered
            terms.push_back(makeBinOp(ExprKind::Add, a->clone(), b->clone()));
            terms.push_back(makeBinOp(ExprKind::Sub, a->clone(), b->clone()));
        }
    }

    static const ExprKind ops[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    std::vector<ExprPtr> conditions;
    for (const auto& lhs : terms) {
        for (const auto& rhs : atoms) {
            for (ExprKind op : ops) conditions.push_back(makeCompare(op, lhs->clone(), rhs->clone()));
        }
    }
    return conditions;
}

int evalOnSnapshot(const Expr& e, const std::vector<int>& vars) {
    switch (e.kind) {
        case ExprKind::IntConst: return e.intValue;
        case ExprKind::Var: return vars[static_cast<size_t>(e.varIndex)];
        default: break;
    }
    // Same wraparound as the interpreter's Add/Sub on the ranges used here.
    long a = evalOnSnapshot(*e.lhs, vars), b = evalOnSnapshot(*e.rhs, vars);
    switch (e.kind) {
        case ExprKind::Add: return static_cast<int>(a + b);
        case ExprKind::Sub: return static_cast<int>(a - b);
        case ExprKind::Lt: return a < b;
        case ExprKind::Gt: return a > b;
        case ExprKind::Eq: return a == b;
        case ExprKind::Le: return a <= b;
        case ExprKind::Ge: return a >= b;
        case ExprKind::Ne: return a != b;
        default: return 0;  // never enumerated
    }
}

std::vector<ExprPtr> dedupeByTruthVector(std::vector<ExprPtr> conditions, const std::vector<std::vector<int>>& snapshots) {
    std::unordered_set<std::string> seen;
    std::vector<ExprPtr> representatives;
    std::string key(snapshots.size(), '0');
    for (auto& cond : conditions) {
        for (size_t k = 0; k < snapshots.size(); ++k) key[k] = evalOnSnapshot(*cond, snapshots[k]) != 0 ? '1' : '0';
        if (seen.insert(key).second) representatives.push_back(std::move(cond));
    }
    return representatives;
}

}  // namespace algosearch
