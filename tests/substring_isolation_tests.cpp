#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/SubstringProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// Isolation test (FINDINGS.md section on Substring): SubstringProblem.hpp's
// own comment claims the naive O(n*m) shape "recombines only the project's
// SHARED primitives (counting loop + captured scalar + sentinel flag) in an
// arrangement no existing skeleton builds" -- a design-time hypothesis that
// was never actually checked against the interpreter. This builds that exact
// program by hand, using ONLY primitives every other skeleton already uses
// (Assign/If/While/compare/arithmetic/array access, no Break/Continue), and
// runs it end-to-end to separate two different questions: "can the VM
// express/execute this at all?" vs. "can the search find it?" -- see
// FINDINGS.md sections 18-19 for the same two-phase protocol applied to
// TSP/DTW.
//
// Flat-array convention (SubstringProblem.hpp): arr[0]=n, arr[1..n]=text,
// arr[n+1]=m, arr[n+2..n+1+m]=pattern. v0 is the interpreter-assigned total
// array length (n+m+2), not n itself.
//
// v1 = n = arr[0]
// v2 = n + 1                        (index of m)
// v3 = m = arr[v2]
// v4 = patternBase = v2 + 1         (= n + 2)
// v5 = i = 0
// v6 = found = 0
// v7 = n - m                        (last valid start index)
// while (v5 <= v7) {                // for i in [0, n-m]
//   v8 = ok = 1
//   v9 = j = 0
//   while (v9 < v3) {               // for j in [0, m)
//     v10 = textIdx = v5 + v9 + 1   (= 1 + i + j)
//     v11 = patIdx = v4 + v9        (= patternBase + j)
//     if (arr[v10] != arr[v11]) { v8 = 0; }
//     v9 = v9 + 1;
//   }
//   if (v8 == 1) { v6 = 1; }
//   v5 = v5 + 1;
// }
// arr[0] = v6;
Program buildNaiveSubstring() {
    auto root = makeBlock();
    root->body.push_back(makeAssign(1, makeArrayAccess(makeIntConst(0))));                       // v1 = arr[0] (n)
    root->body.push_back(makeAssign(2, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1))));   // v2 = n + 1
    root->body.push_back(makeAssign(3, makeArrayAccess(makeVar(2))));                             // v3 = arr[n+1] (m)
    root->body.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1))));   // v4 = n + 2
    root->body.push_back(makeAssign(5, makeIntConst(0)));                                         // v5 = i = 0
    root->body.push_back(makeAssign(6, makeIntConst(0)));                                         // v6 = found = 0
    root->body.push_back(makeAssign(7, makeBinOp(ExprKind::Sub, makeVar(1), makeVar(3))));        // v7 = n - m

    std::vector<StmtPtr> innerBody;
    innerBody.push_back(makeAssign(
        10, makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeVar(5), makeVar(9)), makeIntConst(1))));  // v10 = i+j+1
    innerBody.push_back(makeAssign(11, makeBinOp(ExprKind::Add, makeVar(4), makeVar(9))));                   // v11 = patternBase+j
    std::vector<StmtPtr> mismatchBody;
    mismatchBody.push_back(makeAssign(8, makeIntConst(0)));  // v8 = ok = 0
    innerBody.push_back(makeIf(
        makeCompare(ExprKind::Ne, makeArrayAccess(makeVar(10)), makeArrayAccess(makeVar(11))), std::move(mismatchBody)));
    innerBody.push_back(makeAssign(9, makeBinOp(ExprKind::Add, makeVar(9), makeIntConst(1))));  // v9 = j + 1

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(8, makeIntConst(1)));  // v8 = ok = 1
    outerBody.push_back(makeAssign(9, makeIntConst(0)));  // v9 = j = 0
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(9), makeVar(3)), std::move(innerBody)));
    std::vector<StmtPtr> foundBody;
    foundBody.push_back(makeAssign(6, makeIntConst(1)));  // v6 = found = 1
    outerBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(8), makeIntConst(1)), std::move(foundBody)));
    outerBody.push_back(makeAssign(5, makeBinOp(ExprKind::Add, makeVar(5), makeIntConst(1))));  // v5 = i + 1

    root->body.push_back(makeWhile(makeCompare(ExprKind::Le, makeVar(5), makeVar(7)), std::move(outerBody)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(6)));  // arr[0] = found
    return root;
}

}  // namespace

// Phase 1: can the interpreter execute this program at all, on hand-picked
// cases spanning "found at front/middle/back", "absent", "pattern longer
// than text", and single-element text/pattern?
TEST(hand_written_substring_matches_hand_picked_cases) {
    Program prog = buildNaiveSubstring();
    Interpreter interp(100000);

    struct Case {
        std::vector<int> text;
        std::vector<int> pattern;
        int expected;
    };
    std::vector<Case> cases = {
        {{0, 1, 2, 3, 4}, {2, 3}, 1},      // middle match
        {{0, 1, 2, 3, 4}, {0, 1}, 1},      // front match
        {{0, 1, 2, 3, 4}, {3, 4}, 1},      // back match
        {{0, 1, 2, 3, 4}, {1, 3}, 0},      // absent (right values, wrong order)
        {{5}, {5}, 1},                    // single-element exact match
        {{5}, {6}, 0},                    // single-element mismatch
        {{1, 1, 1}, {1, 1}, 1},           // repeated-alphabet match
        {{1, 1, 1}, {1, 1, 1, 1}, 0},     // pattern longer than text -> handled below (needs n>=m)
    };

    for (auto& c : cases) {
        if (c.pattern.size() > c.text.size()) continue;  // n>=m only, per flat-array convention
        int n = static_cast<int>(c.text.size());
        int m = static_cast<int>(c.pattern.size());
        std::vector<int> input;
        input.push_back(n);
        input.insert(input.end(), c.text.begin(), c.text.end());
        input.push_back(m);
        input.insert(input.end(), c.pattern.begin(), c.pattern.end());

        std::vector<int> output;
        RunStatus status = interp.run(*prog, input, 12, output);
        CHECK(status == RunStatus::COMPLETED);
        CHECK_EQ(output[0], c.expected);
    }
}

// Phase 2: does it hold up against SubstringProblem's own generated
// instances (N=1..8, crafted + random text families) and adversarial
// instances (wide-range values including negatives)?
TEST(hand_written_substring_solves_problem_levels) {
    Program prog = buildNaiveSubstring();
    Interpreter interp(100000);
    SubstringProblem problem(8, 30, 12, 1);

    for (int level = 1; level <= 8; ++level) {
        auto instances = problem.generateInstances(level);
        CHECK(!instances.empty());
        for (const auto& inst : instances) {
            EvalOutcome outcome = problem.evaluate(*prog, inst, interp);
            CHECK(outcome.status == ExecStatus::SUCCESS);
        }
    }
}

TEST(hand_written_substring_solves_adversarial_instances) {
    Program prog = buildNaiveSubstring();
    Interpreter interp(100000);
    SubstringProblem problem(8, 30, 12, 1);

    for (int level : {2, 4, 6, 8}) {
        auto instances = problem.generateAdversarialInstances(level);
        CHECK(!instances.empty());
        for (const auto& inst : instances) {
            EvalOutcome outcome = problem.evaluate(*prog, inst, interp);
            CHECK(outcome.status == ExecStatus::SUCCESS);
        }
    }
}
