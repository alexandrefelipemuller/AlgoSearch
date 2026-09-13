#include <algorithm>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/SortingProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// Manually hand-written Bubble Sort in the AST form, to validate the
// interpreter end-to-end before relying on automatic discovery.
//
// v0 = n (implicit initial value)
// v1 = i, v2 = j
// for i in [0, n): for j in [0, n-1-i): if arr[j] > arr[j+1]: swap
//
// while (v1 < v0) {
//   v2 = 0;
//   while (v2 < (v0 - v1) - 1) {
//     if (arr[v2] > arr[v2 + 1]) { swap(arr[v2], arr[v2+1]); }
//     v2 = v2 + 1;
//   }
//   v1 = v1 + 1;
// }
Program buildBubbleSort() {
    auto root = makeBlock();
    root->body.push_back(makeAssign(1, makeIntConst(0)));  // v1 = 0

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(2, makeIntConst(0)));  // v2 = 0

    std::vector<StmtPtr> ifBody;
    ifBody.push_back(makeSwap(makeVar(2), makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1))));

    std::vector<StmtPtr> innerBody;
    innerBody.push_back(makeIf(
        makeCompare(ExprKind::Gt, makeArrayAccess(makeVar(2)), makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1)))),
        std::move(ifBody)));
    innerBody.push_back(makeAssign(2, makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1))));

    auto innerBound = makeBinOp(ExprKind::Sub, makeBinOp(ExprKind::Sub, makeVar(0), makeVar(1)), makeIntConst(1));
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(2), std::move(innerBound)), std::move(innerBody)));
    outerBody.push_back(makeAssign(1, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1))));

    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(1), makeVar(0)), std::move(outerBody)));
    return root;
}

}  // namespace

TEST(hand_written_bubble_sort_sorts_arrays) {
    Program bubbleSort = buildBubbleSort();
    Interpreter interp(100000);

    std::vector<std::vector<int>> cases = {{3, 1, 2}, {5, 4, 3, 2, 1}, {1}, {}, {2, 2, 1, 1}, {0, 0, 0}};
    for (auto& input : cases) {
        std::vector<int> output;
        RunStatus status = interp.run(*bubbleSort, input, 4, output);
        CHECK(status == RunStatus::COMPLETED);
        std::vector<int> expected = input;
        std::sort(expected.begin(), expected.end());
        CHECK_EQ(output, expected);
    }
}

TEST(hand_written_bubble_sort_solves_sorting_problem_levels) {
    Program bubbleSort = buildBubbleSort();
    Interpreter interp(100000);
    SortingProblem problem(6, 40, 4, 1);

    for (int level = 1; level <= 5; ++level) {
        auto instances = problem.generateInstances(level);
        for (const auto& inst : instances) {
            EvalOutcome outcome = problem.evaluate(*bubbleSort, inst, interp);
            CHECK(outcome.status == ExecStatus::SUCCESS);
        }
    }
}
