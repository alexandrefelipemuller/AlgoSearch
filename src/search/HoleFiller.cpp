#include "search/HoleFiller.hpp"

#include <algorithm>
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

namespace {

// Cheapest way to visit every vertex in `remaining` starting from `at` and
// return to vertex 0, on top of `costSoFar`; weights of 0 mean "no edge".
// Returns INT_MAX-ish when no valid completion exists.
long bestCompletion(const std::vector<int>& arr, int nv, int mBase, int at, long costSoFar, std::vector<int>& remaining,
                    long cutoff) {
    auto weight = [&](int from, int to) { return arr[static_cast<size_t>(mBase + from * nv + to)]; };
    if (costSoFar >= cutoff) return cutoff;  // cannot beat what is already known
    if (remaining.empty()) {
        int back = weight(at, 0);
        return back > 0 ? costSoFar + back : cutoff;
    }
    long best = cutoff;
    for (size_t k = 0; k < remaining.size(); ++k) {
        int next = remaining[k];
        int w = weight(at, next);
        if (w <= 0) continue;
        std::swap(remaining[k], remaining.back());
        remaining.pop_back();
        best = std::min(best, bestCompletion(arr, nv, mBase, next, costSoFar + w, remaining, best));
        remaining.push_back(next);
        std::swap(remaining[k], remaining.back());
    }
    return best;
}

}  // namespace

bool tspPruneIsSafe(const std::vector<int>& vars, const std::vector<int>& arr) {
    int nv = vars[1], uBase = vars[3], mBase = vars[6], best = vars[7], c = vars[9], cost = vars[11];
    std::vector<int> remaining;
    for (int k = 0; k < nv; ++k) {
        if (k != c && arr[static_cast<size_t>(uBase + k)] == 0) remaining.push_back(k);
    }
    // Only a completion STRICTLY below best makes pruning unsafe, so best is
    // the cutoff: bestCompletion returns best when nothing beats it.
    return bestCompletion(arr, nv, mBase, c, cost, remaining, best) >= best;
}

}  // namespace algosearch
