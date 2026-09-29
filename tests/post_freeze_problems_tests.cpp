#include <set>
#include <string>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/CountDescentsProblem.hpp"
#include "problems/HasAdjacentEqualProblem.hpp"
#include "problems/HasDuplicateProblem.hpp"
#include "problems/LongestRunProblem.hpp"
#include "problems/MaxAdjacentGapProblem.hpp"
#include "problems/StrictlyIncreasingProblem.hpp"
#include "problems/InversionCountProblem.hpp"
#include "problems/IsSortedProblem.hpp"
#include "problems/PalindromeProblem.hpp"
#include "search/Benchmark.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

ExprPtr var(int v) { return makeVar(v); }
ExprPtr lit(int c) { return makeIntConst(c); }
ExprPtr add(ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Add, std::move(a), std::move(b)); }
ExprPtr sub(ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Sub, std::move(a), std::move(b)); }
ExprPtr at(ExprPtr i) { return makeArrayAccess(std::move(i)); }
std::vector<StmtPtr> one(StmtPtr s) {
    std::vector<StmtPtr> v;
    v.push_back(std::move(s));
    return v;
}

// Hand-written reference programs (v0 = n; v1, v2 loop vars; v3 result).
Program palindromeProgram() {
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(1)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeAssign(2, sub(var(0), lit(1))));
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Ne, at(var(1)), at(var(2))), one(makeAssign(3, lit(0)))));
    body.push_back(makeAssign(1, add(var(1), lit(1))));
    body.push_back(makeAssign(2, sub(var(2), lit(1))));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, var(1), var(2)), std::move(body)));
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    return p;
}

// Triangular double loop with `pairStmt` at the innermost point.
Program triangular(StmtPtr init, StmtPtr pairStmt) {
    auto p = makeBlock();
    p->body.push_back(std::move(init));
    std::vector<StmtPtr> inner;
    inner.push_back(std::move(pairStmt));
    inner.push_back(makeAssign(2, add(var(2), lit(1))));
    std::vector<StmtPtr> outer;
    outer.push_back(makeAssign(2, add(var(1), lit(1))));
    outer.push_back(makeWhile(makeCompare(ExprKind::Lt, var(2), var(0)), std::move(inner)));
    outer.push_back(makeAssign(1, add(var(1), lit(1))));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    return p;
}

Program inversionCountProgram() {
    return triangular(makeAssign(3, lit(0)), makeIf(makeCompare(ExprKind::Gt, at(var(1)), at(var(2))),
                                                    one(makeAssign(3, add(var(3), lit(1))))));
}

Program hasDuplicateProgram() {
    return triangular(makeAssign(3, lit(0)),
                      makeIf(makeCompare(ExprKind::Eq, at(var(1)), at(var(2))), one(makeAssign(3, lit(1)))));
}

Program isSortedProgram() {
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(1)));
    p->body.push_back(makeAssign(1, lit(1)));
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Gt, at(sub(var(1), lit(1))), at(var(1))), one(makeAssign(3, lit(0)))));
    body.push_back(makeAssign(1, add(var(1), lit(1))));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, var(1), var(0)), std::move(body)));
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    return p;
}

// Single pass i = 1..n-1 with `body` statements, result v3 written to arr[0].
Program adjacentPass(std::vector<StmtPtr> init, std::vector<StmtPtr> body) {
    auto p = makeBlock();
    for (auto& st : init) p->body.push_back(std::move(st));
    body.push_back(makeAssign(1, add(var(1), lit(1))));
    p->body.push_back(makeAssign(1, lit(1)));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, var(1), var(0)), std::move(body)));
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    return p;
}
ExprPtr prev() { return at(sub(var(1), lit(1))); }
ExprPtr cur() { return at(var(1)); }

void checkProblem(const ScalarFromArrayProblem& problem, const Stmt& reference) {
    Interpreter interp(1000000);
    for (int n = 1; n <= 8; ++n) {
        std::set<int> labels;
        auto instances = problem.generateInstances(n);
        CHECK(!instances.empty());
        for (const auto& inst : instances) {
            CHECK_EQ(inst.expectedOutput[0], problem.answer(inst.input));  // labels are ground truth
            labels.insert(inst.expectedOutput[0]);
            CHECK(problem.evaluate(reference, inst, interp).status == ExecStatus::SUCCESS);
        }
        if (n >= 2) CHECK(labels.size() >= 2);  // no single-answer shortcut
        for (const auto& inst : problem.generateAdversarialInstances(n)) {
            CHECK(problem.evaluate(reference, inst, interp).status == ExecStatus::SUCCESS);
        }
    }
    std::string reason;
    CHECK(verifyGeneralization(problem, reference, 10000000, reason, false));  // includes the exhaustive check
}

}  // namespace

TEST(palindrome_problem_labels_and_reference_solution) {
    PalindromeProblem problem;
    CHECK_EQ(problem.answer({1, 2, 1}), 1);
    CHECK_EQ(problem.answer({1, 2, 2}), 0);
    CHECK(problem.isSolution({1, 2, 1}, {1, 0, 0}));
    CHECK(!problem.isSolution({1, 2, 1}, {0, 2, 1}));
    checkProblem(problem, *palindromeProgram());
}

TEST(inversion_count_problem_labels_and_reference_solution) {
    InversionCountProblem problem;
    CHECK_EQ(problem.answer({3, 1, 2}), 2);
    CHECK_EQ(problem.answer({1, 1, 1}), 0);
    CHECK(problem.isSolution({3, 1, 2}, {2, 1, 2}));
    checkProblem(problem, *inversionCountProgram());
}

TEST(has_duplicate_problem_labels_and_reference_solution) {
    HasDuplicateProblem problem;
    CHECK_EQ(problem.answer({4, 1, 4}), 1);
    CHECK_EQ(problem.answer({4, 1, 2}), 0);
    checkProblem(problem, *hasDuplicateProgram());
}

TEST(is_sorted_problem_labels_and_reference_solution) {
    IsSortedProblem problem;
    CHECK_EQ(problem.answer({1, 1, 2}), 1);
    CHECK_EQ(problem.answer({1, 3, 2}), 0);
    checkProblem(problem, *isSortedProgram());
}

TEST(post_freeze_problems_exhaustive_check_rejects_a_near_miss) {
    // A palindrome check that forgets the middle-out walk and compares only
    // the two ends passes many sampled instances but not every small array.
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(1)));
    p->body.push_back(
        makeIf(makeCompare(ExprKind::Ne, at(lit(0)), at(sub(var(0), lit(1)))), one(makeAssign(3, lit(0)))));
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    PalindromeProblem problem;
    std::string reason;
    CHECK(!verifyGeneralization(problem, *p, 1000000, reason, false));
}

TEST(count_descents_problem_labels_and_reference_solution) {
    CountDescentsProblem problem;
    CHECK_EQ(problem.answer({3, 1, 2, 0}), 2);
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Gt, prev(), cur()), one(makeAssign(3, add(var(3), lit(1))))));
    checkProblem(problem, *adjacentPass(one(makeAssign(3, lit(0))), std::move(body)));
}

TEST(strictly_increasing_problem_labels_and_reference_solution) {
    StrictlyIncreasingProblem problem;
    CHECK_EQ(problem.answer({1, 2, 3}), 1);
    CHECK_EQ(problem.answer({1, 2, 2}), 0);
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Ge, prev(), cur()), one(makeAssign(3, lit(0)))));
    checkProblem(problem, *adjacentPass(one(makeAssign(3, lit(1))), std::move(body)));
}

TEST(has_adjacent_equal_problem_labels_and_reference_solution) {
    HasAdjacentEqualProblem problem;
    CHECK_EQ(problem.answer({1, 2, 1}), 0);  // a repeat, but not adjacent
    CHECK_EQ(problem.answer({1, 2, 2}), 1);
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Eq, prev(), cur()), one(makeAssign(3, lit(1)))));
    checkProblem(problem, *adjacentPass(one(makeAssign(3, lit(0))), std::move(body)));
}

TEST(longest_run_problem_labels_and_reference_solution) {
    LongestRunProblem problem;
    CHECK_EQ(problem.answer({1, 1, 2, 2, 2, 1}), 3);
    CHECK_EQ(problem.answer({4}), 1);
    // v3 = best, v4 = current run.
    std::vector<StmtPtr> init;
    init.push_back(makeAssign(3, lit(1)));
    init.push_back(makeAssign(4, lit(1)));
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Eq, prev(), cur()), one(makeAssign(4, add(var(4), lit(1)))),
                          one(makeAssign(4, lit(1)))));
    body.push_back(makeIf(makeCompare(ExprKind::Gt, var(4), var(3)), one(makeAssign(3, var(4)))));
    checkProblem(problem, *adjacentPass(std::move(init), std::move(body)));
}

TEST(max_adjacent_gap_problem_labels_and_reference_solution) {
    MaxAdjacentGapProblem problem;
    CHECK_EQ(problem.answer({1, 5, 4}), 4);
    CHECK_EQ(problem.answer({7}), 0);
    // v3 = best, v4 = |difference|.
    std::vector<StmtPtr> body;
    body.push_back(makeAssign(4, sub(cur(), prev())));
    body.push_back(makeIf(makeCompare(ExprKind::Lt, var(4), lit(0)), one(makeAssign(4, sub(lit(0), var(4))))));
    body.push_back(makeIf(makeCompare(ExprKind::Gt, var(4), var(3)), one(makeAssign(3, var(4)))));
    checkProblem(problem, *adjacentPass(one(makeAssign(3, lit(0))), std::move(body)));
}
