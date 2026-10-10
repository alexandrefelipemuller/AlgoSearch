#include <vector>

#include "interpreter/Interpreter.hpp"
#include "problems/ListQueryProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

ExprPtr var(int v) { return makeVar(v); }
ExprPtr lit(int c) { return makeIntConst(c); }
ExprPtr add(ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Add, std::move(a), std::move(b)); }
ExprPtr cmp(ExprKind k, ExprPtr a, ExprPtr b) { return makeCompare(k, std::move(a), std::move(b)); }

template <typename... S>
std::vector<StmtPtr> stmts(S... s) {
    std::vector<StmtPtr> v;
    (v.push_back(std::move(s)), ...);
    return v;
}

// v1 = head; v3 = steps; walk while v1 is a node (0 <= v1 < n, n = v0 - 1),
// at most v0 steps, running `each` before every advance; then `after`.
Program walkProgram(std::vector<StmtPtr> each, std::vector<StmtPtr> after) {
    auto p = makeBlock();
    p->body.push_back(makeAssign(4, add(var(0), lit(-1))));  // n
    p->body.push_back(makeAssign(1, makeArrayAccess(lit(0))));
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(5, lit(0)));
    std::vector<StmtPtr> body;
    body.push_back(makeIf(cmp(ExprKind::Lt, var(1), lit(0)), stmts(makeBreak())));
    body.push_back(makeIf(cmp(ExprKind::Ge, var(1), var(4)), stmts(makeBreak())));
    for (auto& s : each) body.push_back(std::move(s));
    body.push_back(makeAssign(1, makeArrayAccess(add(lit(1), var(1)))));
    body.push_back(makeAssign(3, add(var(3), lit(1))));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(3), var(0)), std::move(body)));
    for (auto& s : after) p->body.push_back(std::move(s));
    p->body.push_back(makeAssignArray(lit(0), var(5)));
    return p;
}

void checkReference(const ListQueryProblem& problem, const Stmt& reference) {
    Interpreter interp(1000000);
    for (int n = 1; n <= 7; ++n) {
        for (const auto& inst : problem.generateInstances(n)) {
            CHECK_EQ(inst.expectedOutput[0], problem.answer(inst.input));
            CHECK(problem.evaluate(reference, inst, interp).status == ExecStatus::SUCCESS);
        }
        for (const auto& inst : problem.generateAdversarialInstances(n)) {
            CHECK(problem.evaluate(reference, inst, interp).status == ExecStatus::SUCCESS);
        }
    }
}

}  // namespace

// Lists: {head, next...}; n = 3, null = 3.
// {0, 1, 2, 3}: 0 -> 1 -> 2 -> null.   {0, 1, 2, 1}: 0 -> 1 -> 2 -> 1 (cycle of 2).
// {2, 3, 3, 0}: 2 -> 0 -> null.         {3, 0, 0, 0}: empty.

TEST(walk_length_problem_labels_and_reference_solution) {
    WalkLengthProblem problem;
    CHECK_EQ(problem.answer({0, 1, 2, 3}), 3);
    CHECK_EQ(problem.answer({0, 1, 2, 1}), 4);  // never ends: n + 1
    CHECK_EQ(problem.answer({3, 0, 0, 0}), 0);
    checkReference(problem, *walkProgram(stmts(), stmts(makeAssign(5, var(3)))));
}

TEST(visits_zero_problem_labels_and_reference_solution) {
    VisitsZeroProblem problem;
    CHECK_EQ(problem.answer({2, 3, 3, 0}), 1);
    CHECK_EQ(problem.answer({1, 1, 2, 3}), 0);
    checkReference(problem, *walkProgram(stmts(makeIf(cmp(ExprKind::Eq, var(1), lit(0)), stmts(makeAssign(5, lit(1))))),
                                         stmts()));
}

TEST(cycle_length_problem_labels_and_reference_solution) {
    CycleLengthProblem problem;
    CHECK_EQ(problem.answer({0, 1, 2, 1}), 2);
    CHECK_EQ(problem.answer({0, 1, 2, 3}), 0);
    // After v0 steps p is on the cycle; walk it once more counting until p returns.
    auto p = walkProgram(stmts(), stmts());
    p->body.pop_back();  // drop the write-back; re-add below
    std::vector<StmtPtr> onCycle;
    onCycle.push_back(makeAssign(6, var(1)));
    onCycle.push_back(makeAssign(1, makeArrayAccess(add(lit(1), var(1)))));
    onCycle.push_back(makeAssign(5, lit(1)));
    onCycle.push_back(makeWhile(cmp(ExprKind::Ne, var(1), var(6)),
                                stmts(makeAssign(1, makeArrayAccess(add(lit(1), var(1)))), makeAssign(5, add(var(5), lit(1))))));
    p->body.push_back(makeIf(cmp(ExprKind::Le, lit(0), var(1)),
                             stmts(makeIf(cmp(ExprKind::Lt, var(1), var(4)), std::move(onCycle)))));
    p->body.push_back(makeAssignArray(lit(0), var(5)));
    checkReference(problem, *p);
}

TEST(last_node_problem_labels_and_reference_solution) {
    LastNodeProblem problem;
    CHECK_EQ(problem.answer({0, 1, 2, 3}), 2);
    CHECK_EQ(problem.answer({0, 1, 2, 1}), 3);  // cyclic: n
    CHECK_EQ(problem.answer({3, 0, 0, 0}), 3);  // empty: n
    // v5 = the node before each advance; n when the walk never ended.
    auto p = walkProgram(stmts(makeAssign(5, var(1))), stmts());
    p->body.pop_back();
    p->body.insert(p->body.begin() + 3, makeAssign(5, var(4)));  // n for an empty list
    p->body.erase(p->body.begin() + 4);                           // drop the v5 = 0 init
    p->body.push_back(makeIf(cmp(ExprKind::Le, lit(0), var(1)),
                             stmts(makeIf(cmp(ExprKind::Lt, var(1), var(4)), stmts(makeAssign(5, var(4)))))));
    p->body.push_back(makeAssignArray(lit(0), var(5)));
    checkReference(problem, *p);
}

TEST(cycle_entry_and_second_last_node_labels) {
    // Section 90 rulers: labels only (their reference programs would need
    // the very pieces the rulers measure the absence of).
    CycleEntryProblem entry;
    CHECK_EQ(entry.answer({0, 1, 2, 1}), 1);  // 0 -> 1 -> 2 -> 1: enters at 1
    CHECK_EQ(entry.answer({0, 1, 2, 3}), 3);  // ends at null: n
    SecondLastNodeProblem second;
    CHECK_EQ(second.answer({0, 1, 2, 3}), 1);  // 0 -> 1 -> 2 -> null
    CHECK_EQ(second.answer({2, 3, 3, 3}), 3);  // one node only: n
    CHECK_EQ(second.answer({0, 1, 2, 1}), 3);  // cyclic: n
    for (int n = 1; n <= 6; ++n) {
        for (const auto& inst : entry.generateInstances(n)) CHECK_EQ(inst.expectedOutput[0], entry.answer(inst.input));
        for (const auto& inst : second.generateInstances(n)) CHECK_EQ(inst.expectedOutput[0], second.answer(inst.input));
    }
}
