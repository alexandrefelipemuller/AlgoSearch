#include <cstdlib>
#include <string>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/FactorialProblem.hpp"
#include "problems/PowerProblem.hpp"
#include "search/Benchmark.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// a = 1; i = 1; while (i <= arr[0]) { a = a * F; i = i + 1; } arr[0] = a;
// F = i (factorial) or arr[0] with bound arr[1] (power): native Mul.
Program buildNativeMulLoop(bool power) {
    auto root = makeBlock();
    root->body.push_back(makeAssign(1, makeIntConst(1)));
    root->body.push_back(makeAssign(2, makeIntConst(1)));
    std::vector<StmtPtr> body;
    ExprPtr factor = power ? makeArrayAccess(makeIntConst(0)) : makeVar(2);
    body.push_back(makeAssign(1, makeBinOp(ExprKind::Mul, makeVar(1), std::move(factor))));
    body.push_back(makeAssign(2, makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1))));
    ExprPtr bound = makeArrayAccess(makeIntConst(power ? 1 : 0));
    root->body.push_back(makeWhile(makeCompare(ExprKind::Le, makeVar(2), std::move(bound)), std::move(body)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(1)));
    return root;
}

// The repeatedmultiply skeleton's add-based multiply, drawn from the generator.
Program drawRepeatedMultiply(const Problem& problem, int numVars) {
    setenv("ALGOSEARCH_FORCE_SKELETON", "repeatedmultiply", 1);
    GenConfig cfg;
    cfg.numVars = numVars;
    cfg.maxLoops = 2;
    cfg.loopAccumulatorBias = 1.0;
    Random rng(3);
    Interpreter interp(100000);
    for (int i = 0; i < 2000; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        bool ok = true;
        for (const auto& inst : problem.generateInstances(4)) {
            if (problem.evaluate(*p, inst, interp).status != ExecStatus::SUCCESS) ok = false;
        }
        if (ok) {
            unsetenv("ALGOSEARCH_FORCE_SKELETON");
            return p;
        }
    }
    unsetenv("ALGOSEARCH_FORCE_SKELETON");
    return nullptr;
}

}  // namespace

TEST(factorial_problem_instances_stay_within_int_domain) {
    FactorialProblem problem(10, 40, 5);
    for (int level = 1; level <= 20; ++level) {
        for (const auto& inst : problem.generateInstances(level)) {
            CHECK(inst.input[0] >= 0 && inst.input[0] <= 12);
            CHECK(inst.expectedOutput[0] >= 1);
        }
    }
    CHECK_EQ(problem.generateBenchmarkInstance(12, 0).expectedOutput[0], 479001600);
    CHECK(problem.generateBenchmarkInstance(13, 0).input.empty());
    CHECK(!problem.isSolution({13}, {1932053504}));  // 13! wrapped mod 2^32 is not an answer
}

TEST(power_problem_instances_stay_within_int_domain) {
    PowerProblem problem(6, 40, 6);
    for (int level = 1; level <= 20; ++level) {
        for (const auto& inst : problem.generateInstances(level)) CHECK(inst.expectedOutput[0] >= 0);
        for (const auto& inst : problem.generateAdversarialInstances(level)) CHECK(inst.expectedOutput[0] >= 0);
    }
    CHECK_EQ(problem.generateBenchmarkInstance(30, 0).expectedOutput[0], 1 << 30);
    CHECK(problem.generateBenchmarkInstance(31, 0).input.empty());
}

TEST(native_mul_factorial_and_power_pass_verification) {
    std::string reason;
    CHECK(verifyGeneralization(FactorialProblem(10, 40, 5), *buildNativeMulLoop(false), 200000, reason));
    CHECK(verifyGeneralization(PowerProblem(6, 40, 6), *buildNativeMulLoop(true), 200000, reason));
}

TEST(add_based_repeated_multiply_still_passes_verification) {
    std::string reason;
    FactorialProblem factorial(10, 40, 5);
    Program p = drawRepeatedMultiply(factorial, 5);
    CHECK(p != nullptr);
    if (p) CHECK(verifyGeneralization(factorial, *p, 200000, reason));
}
