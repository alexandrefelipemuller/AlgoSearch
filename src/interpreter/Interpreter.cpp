#include "interpreter/Interpreter.hpp"

#include <functional>

#include <algorithm>
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
    // Second working array (see ExprKind::AuxAccess/StmtKind::AssignAux),
    // always allocated (zero-initialized, same length as arr) so no extra
    // per-run branch is needed -- a program that never emits an aux
    // read/write simply never looks at it, byte-identical to before this
    // field existed.
    std::vector<int> aux;
    long steps = 0;
    long stepLimit = 0;
    Flow flow = Flow::Ok;
    // Recursion extension (see StmtKind::Call). `root` points at the whole
    // program being run so a Call can re-enter it; only ever read, never
    // owned. recursionDepth/maxRecursionDepth guard against a self-call
    // that never shrinks its (lo,hi) range from overflowing the real C++
    // call stack.
    const Stmt* root = nullptr;
    long recursionDepth = 0;
    long maxRecursionDepth = 0;
    // Interpreter::setConditionTrace; null when unused.
    const Stmt* traceSite = nullptr;
    const std::function<void(const std::vector<int>&, const std::vector<int>&)>* traceFn = nullptr;
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
        case ExprKind::AuxAccess: {
            int idx = evalExpr(*e.lhs, env);
            if (aborted(env)) return 0;
            if (idx < 0 || static_cast<size_t>(idx) >= env.aux.size()) {
                env.flow = Flow::Error;
                return 0;
            }
            return env.aux[idx];
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
        case StmtKind::AssignAux: {
            int idx = evalExpr(*s.indexExpr, env);
            if (aborted(env)) return;
            int value = evalExpr(*s.valueExpr, env);
            if (aborted(env)) return;
            if (idx < 0 || static_cast<size_t>(idx) >= env.aux.size()) {
                env.flow = Flow::Error;
                return;
            }
            env.aux[idx] = value;
            break;
        }
        case StmtKind::Call: {
            int lo = evalExpr(*s.indexExpr, env);
            if (aborted(env)) return;
            int hi = evalExpr(*s.indexExpr2, env);
            if (aborted(env)) return;
            if (env.recursionDepth >= env.maxRecursionDepth) {
                env.flow = Flow::Error;
                return;
            }
            // Fresh callee frame: same convention as the initial top-level
            // call (v0 = n), plus v1/v2 seeded from this call's arguments
            // ("lo"/"hi") -- the two extra reserved scalars a recursive
            // divide-and-conquer program uses to know its own sub-range.
            // Ordinary call-by-value: saved and restored around the call so
            // the caller's own locals are untouched by whatever the callee
            // does to its copies.
            std::vector<int> savedVars = env.vars;
            std::fill(env.vars.begin(), env.vars.end(), 0);
            if (!env.vars.empty()) env.vars[0] = static_cast<int>(env.arr.size());
            if (env.vars.size() > 1) env.vars[1] = lo;
            if (env.vars.size() > 2) env.vars[2] = hi;
            env.recursionDepth++;
            execStmt(*env.root, env);
            env.recursionDepth--;
            if (env.flow == Flow::Return) env.flow = Flow::Ok;  // ends the callee's frame only, not the caller's
            if (aborted(env)) return;  // Timeout/Error propagates all the way up
            env.vars = std::move(savedVars);
            break;
        }
        case StmtKind::If: {
            if (&s == env.traceSite) (*env.traceFn)(env.vars, env.arr);
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
                if (&s == env.traceSite) (*env.traceFn)(env.vars, env.arr);
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
    env.aux.assign(input.size(), 0);
    env.steps = 0;
    env.stepLimit = stepLimit_;
    env.flow = Flow::Ok;
    env.root = &program;
    env.recursionDepth = 0;
    env.maxRecursionDepth = maxRecursionDepth_;
    env.traceSite = traceFn_ ? traceSite_ : nullptr;
    env.traceFn = &traceFn_;
    if (seedRecursionBounds_ && numVars > 2) {
        env.vars[1] = 0;
        env.vars[2] = static_cast<int>(input.size());
    }

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
