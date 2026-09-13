#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(interpreter_runs_expressions_and_assign) {
    auto block = makeBlock();
    block->body.push_back(makeAssign(1, makeBinOp(ExprKind::Add, makeVar(0), makeIntConst(10))));
    // v0 is initialized to array length (n); with n=3, v1 should become 13.
    block->body.push_back(makeAssignArray(makeIntConst(0), makeVar(1)));

    Interpreter interp(1000);
    std::vector<int> output;
    RunStatus status = interp.run(*block, {1, 2, 3}, /*numVars=*/4, output);

    CHECK(status == RunStatus::COMPLETED);
    CHECK_EQ(output[0], 13);
}

TEST(interpreter_if_true_and_false_branches) {
    auto makeProgram = [](int constVal) {
        auto block = makeBlock();
        std::vector<StmtPtr> thenBody;
        thenBody.push_back(makeAssign(1, makeIntConst(100)));
        std::vector<StmtPtr> elseBody;
        elseBody.push_back(makeAssign(1, makeIntConst(-100)));
        block->body.push_back(
            makeIf(makeCompare(ExprKind::Gt, makeIntConst(constVal), makeIntConst(0)), std::move(thenBody), std::move(elseBody)));
        block->body.push_back(makeAssignArray(makeIntConst(0), makeVar(1)));
        return block;
    };

    Interpreter interp(1000);
    std::vector<int> output;

    auto trueProgram = makeProgram(5);
    interp.run(*trueProgram, {0}, 4, output);
    CHECK_EQ(output[0], 100);

    auto falseProgram = makeProgram(-5);
    interp.run(*falseProgram, {0}, 4, output);
    CHECK_EQ(output[0], -100);
}

TEST(interpreter_while_loop_sums_via_swap) {
    // while (v1 < v0) { swap(arr[v1], arr[v1]); v1 = v1 + 1; }
    // A no-op-ish loop that just needs to terminate after n iterations.
    auto block = makeBlock();
    block->body.push_back(makeAssign(1, makeIntConst(0)));
    std::vector<StmtPtr> body;
    body.push_back(makeSwap(makeVar(1), makeVar(1)));
    body.push_back(makeAssign(1, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1))));
    block->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(1), makeVar(0)), std::move(body)));

    Interpreter interp(10000);
    std::vector<int> output;
    RunStatus status = interp.run(*block, {5, 4, 3, 2, 1}, 4, output);
    CHECK(status == RunStatus::COMPLETED);
    CHECK_EQ(output, (std::vector<int>{5, 4, 3, 2, 1}));
}

TEST(interpreter_swap_exchanges_elements) {
    auto block = makeBlock();
    block->body.push_back(makeSwap(makeIntConst(0), makeIntConst(2)));

    Interpreter interp(1000);
    std::vector<int> output;
    interp.run(*block, {10, 20, 30}, 4, output);
    CHECK_EQ(output, (std::vector<int>{30, 20, 10}));
}

TEST(interpreter_return_stops_execution_early) {
    auto block = makeBlock();
    block->body.push_back(makeAssignArray(makeIntConst(0), makeIntConst(999)));
    block->body.push_back(makeReturn());
    block->body.push_back(makeAssignArray(makeIntConst(0), makeIntConst(1)));  // must not run

    Interpreter interp(1000);
    std::vector<int> output;
    RunStatus status = interp.run(*block, {0}, 4, output);
    CHECK(status == RunStatus::COMPLETED);
    CHECK_EQ(output[0], 999);
}

TEST(interpreter_step_limit_triggers_timeout) {
    // Infinite loop: while (0 == 0) {}
    std::vector<StmtPtr> body;  // empty body, condition always true
    auto block = makeBlock();
    block->body.push_back(makeWhile(makeCompare(ExprKind::Eq, makeIntConst(0), makeIntConst(0)), std::move(body)));

    Interpreter interp(500);  // small deterministic step budget
    std::vector<int> output;
    RunStatus status = interp.run(*block, {1, 2, 3}, 4, output);
    CHECK(status == RunStatus::TIMEOUT);
}

TEST(interpreter_out_of_bounds_is_runtime_error) {
    auto block = makeBlock();
    block->body.push_back(makeAssignArray(makeIntConst(50), makeIntConst(1)));

    Interpreter interp(1000);
    std::vector<int> output;
    RunStatus status = interp.run(*block, {1, 2, 3}, 4, output);
    CHECK(status == RunStatus::RUNTIME_ERROR);
}
