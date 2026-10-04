#include "search/TailFiller.hpp"

#include <optional>

#include "ast/Expr.hpp"

namespace algosearch {

namespace {

struct EndState {
    bool completed = false;
    std::vector<int> arr;
    std::vector<int> vars;
};

// The index expressions a tail may read arr at.
std::vector<ExprPtr> tailIndices() {
    auto n = [] { return makeVar(0); };
    std::vector<ExprPtr> out;
    for (int c = 0; c <= 2; ++c) out.push_back(makeIntConst(c));
    out.push_back(makeBinOp(ExprKind::Sub, n(), makeIntConst(1)));
    out.push_back(makeBinOp(ExprKind::Sub, n(), makeIntConst(2)));
    out.push_back(makeBinOp(ExprKind::Div, n(), makeIntConst(2)));
    out.push_back(makeBinOp(ExprKind::Div, makeBinOp(ExprKind::Sub, n(), makeIntConst(1)), makeIntConst(2)));
    return out;
}

std::vector<ExprPtr> tailValues(int numVars) {
    std::vector<ExprPtr> out;
    for (int c = 0; c <= 2; ++c) out.push_back(makeIntConst(c));
    for (int v = 1; v < numVars; ++v) out.push_back(makeVar(v));
    for (auto& idx : tailIndices()) out.push_back(makeArrayAccess(std::move(idx)));
    return out;
}

// Value of a tail expression on an end state; nullopt on an out-of-range
// read or a division by zero (the interpreter would raise a runtime error).
std::optional<int> evalTail(const Expr& e, const EndState& s) {
    switch (e.kind) {
        case ExprKind::IntConst:
            return e.intValue;
        case ExprKind::Var:
            return s.vars[static_cast<size_t>(e.varIndex)];
        case ExprKind::ArrayAccess: {
            auto i = evalTail(*e.lhs, s);
            if (!i || *i < 0 || *i >= static_cast<int>(s.arr.size())) return std::nullopt;
            return s.arr[static_cast<size_t>(*i)];
        }
        case ExprKind::Sub:
        case ExprKind::Div: {
            auto a = evalTail(*e.lhs, s), b = evalTail(*e.rhs, s);
            if (!a || !b) return std::nullopt;
            if (e.kind == ExprKind::Sub) return *a - *b;
            if (*b == 0) return std::nullopt;
            return *a / *b;
        }
        default:
            return std::nullopt;
    }
}

bool guardHolds(ExprKind op, int n, int k) {
    switch (op) {
        case ExprKind::Lt: return n < k;
        case ExprKind::Le: return n <= k;
        case ExprKind::Eq: return n == k;
        case ExprKind::Ge: return n >= k;
        default: return n > k;
    }
}

StmtPtr writeOutput(const Expr& value) { return makeAssignArray(makeIntConst(0), value.clone()); }

}  // namespace

Program fillTail(const Stmt& prefix, const std::vector<Instance>& instances, const Problem& problem,
                 const Interpreter& interp) {
    const int numVars = problem.numVars();
    std::vector<EndState> ends(instances.size());
    bool anyCompleted = false;
    for (size_t i = 0; i < instances.size(); ++i) {
        RunStatus rs = interp.run(prefix, instances[i].input, numVars, ends[i].arr, nullptr, &ends[i].vars);
        ends[i].completed = rs == RunStatus::COMPLETED;
        anyCompleted = anyCompleted || ends[i].completed;
    }
    if (!anyCompleted) return nullptr;

    // correct[e][i]: does `arr[0] = values[e]` solve instance i?
    std::vector<ExprPtr> values = tailValues(numVars);
    std::vector<std::vector<char>> correct(values.size(), std::vector<char>(instances.size(), 0));
    std::vector<int> output;
    for (size_t e = 0; e < values.size(); ++e) {
        for (size_t i = 0; i < instances.size(); ++i) {
            if (!ends[i].completed || ends[i].arr.empty()) continue;
            auto v = evalTail(*values[e], ends[i]);
            if (!v) continue;
            output = ends[i].arr;
            output[0] = *v;
            correct[e][i] = problem.isSolution(instances[i].input, output) ? 1 : 0;
        }
    }

    // Unguarded tails.
    int bestCount = -1;
    size_t bestE = 0;
    for (size_t e = 0; e < values.size(); ++e) {
        int c = 0;
        for (char ok : correct[e]) c += ok;
        if (c > bestCount) bestCount = c, bestE = e;
    }
    StmtPtr bestTail = writeOutput(*values[bestE]);

    // Guarded tails: maximize separately on each side of the guard.
    const ExprKind ops[] = {ExprKind::Lt, ExprKind::Le, ExprKind::Eq, ExprKind::Ge, ExprKind::Gt};
    for (ExprKind op : ops) {
        for (int k = 1; k <= 3; ++k) {
            int bestIn = -1, bestOut = -1;
            size_t eIn = 0, eOut = 0;
            for (size_t e = 0; e < values.size(); ++e) {
                int in = 0, out = 0;
                for (size_t i = 0; i < instances.size(); ++i) {
                    int n = static_cast<int>(instances[i].input.size());
                    (guardHolds(op, n, k) ? in : out) += correct[e][i];
                }
                if (in > bestIn) bestIn = in, eIn = e;
                if (out > bestOut) bestOut = out, eOut = e;
            }
            if (eIn != eOut && bestIn + bestOut > bestCount) {
                bestCount = bestIn + bestOut;
                std::vector<StmtPtr> thenBody, elseBody;
                thenBody.push_back(writeOutput(*values[eIn]));
                elseBody.push_back(writeOutput(*values[eOut]));
                bestTail = makeIf(makeCompare(op, makeVar(0), makeIntConst(k)), std::move(thenBody), std::move(elseBody));
            }
        }
    }

    auto program = prefix.clone();
    program->body.push_back(std::move(bestTail));
    return program;
}

}  // namespace algosearch
