#include "interpreter/Interpreter.hpp"

#include <limits>

#include "ast/Expr.hpp"

namespace algosearch {

namespace {

// How a (sub)evaluation ended. Return/timeout/runtime-error are NOT
// exceptional here: randomly generated candidate programs hit out-of-bounds
// accesses and the step limit tens of thousands of times per search round,
// so these are propagated as an ordinary status flag checked after each
// recursive call rather than thrown -- exception unwinding at that frequency
// costs more than the interpretation itself.
enum class Flow : unsigned char { Ok, Return, Continue, Break, Timeout, Error };

struct Environment {
    std::vector<int> vars;
    std::vector<int> arr;
    long steps = 0;
    long stepLimit = 0;
    Flow flow = Flow::Ok;
};

// True once execution must stop unwinding towards run(); every recursive
// call site checks this immediately after returning.
inline bool aborted(const Environment& env) { return env.flow != Flow::Ok; }

inline void tick(Environment& env) {
    env.steps++;
    if (env.steps > env.stepLimit) env.flow = Flow::Timeout;
}

int evalExpr(const Expr& e, Environment& env) {
    tick(env);
    if (aborted(env)) return 0;
    switch (e.kind) {
        case ExprKind::IntConst:
            return e.intValue;
        case ExprKind::Var:
            if (e.varIndex < 0 || static_cast<size_t>(e.varIndex) >= env.vars.size()) {
                env.flow = Flow::Error;
                return 0;
            }
            return env.vars[e.varIndex];
        case ExprKind::ArrayAccess: {
            int idx = evalExpr(*e.lhs, env);
            if (aborted(env)) return 0;
            if (idx < 0 || static_cast<size_t>(idx) >= env.arr.size()) {
                env.flow = Flow::Error;
                return 0;
            }
            return env.arr[idx];
        }
        case ExprKind::Add:
        case ExprKind::Sub:
        case ExprKind::Lt:
        case ExprKind::Gt:
        case ExprKind::Eq:
        case ExprKind::Le:
        case ExprKind::Ge:
        case ExprKind::Ne:
        case ExprKind::Mul:
        case ExprKind::Div:
        case ExprKind::Mod:
        case ExprKind::BitAnd:
        case ExprKind::BitOr:
        case ExprKind::BitXor:
        case ExprKind::Shl:
        case ExprKind::Shr: {
            // Operands are evaluated left-then-right and short-circuit on
            // abort; a completing run ticks both sides exactly as before, so
            // step counts (and hence timeouts) are unchanged.
            int a = evalExpr(*e.lhs, env);
            if (aborted(env)) return 0;
            int b = evalExpr(*e.rhs, env);
            if (aborted(env)) return 0;
            switch (e.kind) {
                case ExprKind::Add: return a + b;
                case ExprKind::Sub: return a - b;
                case ExprKind::Lt: return a < b ? 1 : 0;
                case ExprKind::Gt: return a > b ? 1 : 0;
                case ExprKind::Le: return a <= b ? 1 : 0;
                case ExprKind::Ge: return a >= b ? 1 : 0;
                case ExprKind::Ne: return a != b ? 1 : 0;
                case ExprKind::Mul: {
                    // Randomly generated candidates can compound multiplies
                    // arbitrarily deep; a plain `a * b` risks signed overflow
                    // (UB), unlike Add/Sub whose small-array operands rarely
                    // approach INT_MAX. Widen to 64-bit and treat overflow as
                    // an ordinary runtime error, same bucket as an
                    // out-of-bounds array access -- a generated candidate
                    // that overflows just scores as broken, nothing crashes.
                    long long wide = static_cast<long long>(a) * static_cast<long long>(b);
                    if (wide < std::numeric_limits<int>::min() || wide > std::numeric_limits<int>::max()) {
                        env.flow = Flow::Error;
                        return 0;
                    }
                    return static_cast<int>(wide);
                }
                case ExprKind::Div:
                    // INT_MIN / -1 overflows (UB) the same way Mul can.
                    if (b == 0 || (a == std::numeric_limits<int>::min() && b == -1)) {
                        env.flow = Flow::Error;
                        return 0;
                    }
                    return a / b;
                case ExprKind::Mod:
                    if (b == 0 || (a == std::numeric_limits<int>::min() && b == -1)) {
                        env.flow = Flow::Error;
                        return 0;
                    }
                    return a % b;
                case ExprKind::BitAnd: return a & b;
                case ExprKind::BitOr: return a | b;
                case ExprKind::BitXor: return a ^ b;
                case ExprKind::Shl:
                    if (b < 0 || b >= 32) {
                        env.flow = Flow::Error;
                        return 0;
                    }
                    return static_cast<int>(static_cast<unsigned>(a) << b);
                case ExprKind::Shr:
                    if (b < 0 || b >= 32) {
                        env.flow = Flow::Error;
                        return 0;
                    }
                    return a >> b;
                default: return a == b ? 1 : 0;
            }
        }
    }
    env.flow = Flow::Error;
    return 0;
}

void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env);

void execStmt(const Stmt& s, Environment& env) {
    tick(env);
    if (aborted(env)) return;
    switch (s.kind) {
        case StmtKind::Assign: {
            int value = evalExpr(*s.valueExpr, env);
            if (aborted(env)) return;
            if (s.varIndex < 0 || static_cast<size_t>(s.varIndex) >= env.vars.size()) {
                env.flow = Flow::Error;
                return;
            }
            env.vars[s.varIndex] = value;
            break;
        }
        case StmtKind::AssignArray: {
            int idx = evalExpr(*s.indexExpr, env);
            if (aborted(env)) return;
            int value = evalExpr(*s.valueExpr, env);
            if (aborted(env)) return;
            if (idx < 0 || static_cast<size_t>(idx) >= env.arr.size()) {
                env.flow = Flow::Error;
                return;
            }
            env.arr[idx] = value;
            break;
        }
        case StmtKind::If: {
            int cond = evalExpr(*s.cond, env);
            if (aborted(env)) return;
            if (cond != 0) {
                execBlock(s.body, env);
            } else {
                execBlock(s.elseBody, env);
            }
            break;
        }
        case StmtKind::While: {
            for (;;) {
                int cond = evalExpr(*s.cond, env);
                if (aborted(env) || cond == 0) return;
                tick(env);
                if (aborted(env)) return;
                execBlock(s.body, env);
                if (env.flow == Flow::Continue) {
                    env.flow = Flow::Ok;  // absorbed here -- just re-check this loop's condition
                    continue;
                }
                if (env.flow == Flow::Break) {
                    env.flow = Flow::Ok;  // absorbed here -- exit this loop, resume after it
                    return;
                }
                if (aborted(env)) return;
            }
        }
        case StmtKind::Swap: {
            int i1 = evalExpr(*s.indexExpr, env);
            if (aborted(env)) return;
            int i2 = evalExpr(*s.indexExpr2, env);
            if (aborted(env)) return;
            if (i1 < 0 || static_cast<size_t>(i1) >= env.arr.size() || i2 < 0 ||
                static_cast<size_t>(i2) >= env.arr.size()) {
                env.flow = Flow::Error;
                return;
            }
            std::swap(env.arr[i1], env.arr[i2]);
            break;
        }
        case StmtKind::Return:
            env.flow = Flow::Return;
            return;
        case StmtKind::Continue:
            env.flow = Flow::Continue;
            return;
        case StmtKind::Break:
            env.flow = Flow::Break;
            return;
        case StmtKind::Block:
            execBlock(s.body, env);
            break;
    }
}

void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env) {
    for (const auto& s : stmts) {
        execStmt(*s, env);
        if (aborted(env)) return;
    }
}

}  // namespace

RunStatus Interpreter::run(const Stmt& program, const std::vector<int>& input, int numVars, std::vector<int>& output,
                            long* stepsOut) const {
    // One environment per thread, reused across the millions of runs a search
    // performs: the vars/arr buffers keep their capacity, so a run costs no
    // heap allocation at all once warm. Safe because run() is reentrancy-free
    // (no nested interpretation) and threads never share an Environment.
    thread_local Environment env;
    env.vars.assign(static_cast<size_t>(numVars), 0);
    if (numVars > 0) env.vars[0] = static_cast<int>(input.size());
    env.arr.assign(input.begin(), input.end());
    env.steps = 0;
    env.stepLimit = stepLimit_;
    env.flow = Flow::Ok;

    execStmt(program, env);

    if (stepsOut) *stepsOut = env.steps;

    switch (env.flow) {
        case Flow::Timeout:
            return RunStatus::TIMEOUT;
        case Flow::Error:
        // A Continue/Break that escapes every enclosing while (e.g. one
        // placed directly in the top-level program block) is malformed,
        // not a valid "normal completion" -- score it as broken like any
        // other structurally invalid candidate, not a silent success.
        case Flow::Continue:
        case Flow::Break:
            return RunStatus::RUNTIME_ERROR;
        default:  // Ok, or Return (normal early termination)
            break;
    }

    // Hand the array over instead of copying it; env.arr inherits output's
    // old storage and is refilled by the assign() above on the next run.
    output.swap(env.arr);
    return RunStatus::COMPLETED;
}

}  // namespace algosearch
