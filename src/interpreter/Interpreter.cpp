#include "interpreter/Interpreter.hpp"

#include "ast/Expr.hpp"

namespace algosearch {

namespace {

struct Environment {
    std::vector<int> vars;
    std::vector<int> arr;
    long steps = 0;
    long stepLimit = 0;
};

// Internal unwinding signals. These never escape Interpreter::run.
struct ReturnSignal {};
struct TimeoutSignal {};
struct RuntimeErrorSignal {};

void tick(Environment& env) {
    env.steps++;
    if (env.steps > env.stepLimit) throw TimeoutSignal{};
}

int evalExpr(const Expr& e, Environment& env) {
    tick(env);
    switch (e.kind) {
        case ExprKind::IntConst:
            return e.intValue;
        case ExprKind::Var:
            if (e.varIndex < 0 || static_cast<size_t>(e.varIndex) >= env.vars.size()) throw RuntimeErrorSignal{};
            return env.vars[e.varIndex];
        case ExprKind::ArrayAccess: {
            int idx = evalExpr(*e.lhs, env);
            if (idx < 0 || static_cast<size_t>(idx) >= env.arr.size()) throw RuntimeErrorSignal{};
            return env.arr[idx];
        }
        case ExprKind::Add:
            return evalExpr(*e.lhs, env) + evalExpr(*e.rhs, env);
        case ExprKind::Sub:
            return evalExpr(*e.lhs, env) - evalExpr(*e.rhs, env);
        case ExprKind::Lt:
            return evalExpr(*e.lhs, env) < evalExpr(*e.rhs, env) ? 1 : 0;
        case ExprKind::Gt:
            return evalExpr(*e.lhs, env) > evalExpr(*e.rhs, env) ? 1 : 0;
        case ExprKind::Eq:
            return evalExpr(*e.lhs, env) == evalExpr(*e.rhs, env) ? 1 : 0;
    }
    throw RuntimeErrorSignal{};
}

void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env);

void execStmt(const Stmt& s, Environment& env) {
    tick(env);
    switch (s.kind) {
        case StmtKind::Assign: {
            int value = evalExpr(*s.valueExpr, env);
            if (s.varIndex < 0 || static_cast<size_t>(s.varIndex) >= env.vars.size()) throw RuntimeErrorSignal{};
            env.vars[s.varIndex] = value;
            break;
        }
        case StmtKind::AssignArray: {
            int idx = evalExpr(*s.indexExpr, env);
            int value = evalExpr(*s.valueExpr, env);
            if (idx < 0 || static_cast<size_t>(idx) >= env.arr.size()) throw RuntimeErrorSignal{};
            env.arr[idx] = value;
            break;
        }
        case StmtKind::If: {
            if (evalExpr(*s.cond, env) != 0) {
                execBlock(s.body, env);
            } else {
                execBlock(s.elseBody, env);
            }
            break;
        }
        case StmtKind::While: {
            while (evalExpr(*s.cond, env) != 0) {
                tick(env);
                execBlock(s.body, env);
            }
            break;
        }
        case StmtKind::Swap: {
            int i1 = evalExpr(*s.indexExpr, env);
            int i2 = evalExpr(*s.indexExpr2, env);
            if (i1 < 0 || static_cast<size_t>(i1) >= env.arr.size()) throw RuntimeErrorSignal{};
            if (i2 < 0 || static_cast<size_t>(i2) >= env.arr.size()) throw RuntimeErrorSignal{};
            std::swap(env.arr[i1], env.arr[i2]);
            break;
        }
        case StmtKind::Return:
            throw ReturnSignal{};
        case StmtKind::Block:
            execBlock(s.body, env);
            break;
    }
}

void execBlock(const std::vector<std::unique_ptr<Stmt>>& stmts, Environment& env) {
    for (const auto& s : stmts) execStmt(*s, env);
}

}  // namespace

RunStatus Interpreter::run(const Stmt& program, const std::vector<int>& input, int numVars, std::vector<int>& output) const {
    Environment env;
    env.vars.assign(static_cast<size_t>(numVars), 0);
    if (numVars > 0) env.vars[0] = static_cast<int>(input.size());
    env.arr = input;
    env.stepLimit = stepLimit_;

    try {
        execStmt(program, env);
    } catch (ReturnSignal&) {
        // normal early termination
    } catch (TimeoutSignal&) {
        return RunStatus::TIMEOUT;
    } catch (RuntimeErrorSignal&) {
        return RunStatus::RUNTIME_ERROR;
    }

    output = env.arr;
    return RunStatus::COMPLETED;
}

}  // namespace algosearch
