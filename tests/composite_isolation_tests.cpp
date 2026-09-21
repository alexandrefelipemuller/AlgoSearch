#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/SubstringProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// Isolation test for the Etapa 3 audit's pending action ("prove the
// algorithm before trusting the search"): hand-write the CLEAN, uncorrupted
// version of what the composite-bias operator (FINDINGS.md section 27)
// found at N=3, and check whether it generalizes on its own, independent of
// beam search's growth/selection noise.
//
// v0 is NOT input[0] -- Interpreter::run initializes v0 to input.size() (the
// whole array's length), so n must be captured explicitly via arr[0], same
// as the operator's own "capA = arr[0]" piece.

// Version 1: ONLY the composite catalog's pieces (section 24 / 27), exactly
// as `genCompositeBlock` can draw them -- the "align-match sentinel" relation
// piece is `if (arr[textIdx] == arr[patIdx]) acc = 1`, a flat OR across every
// (i, j) pair with no per-i reset.
//
// v1 = n = arr[0]; v2 = m = arr[v1+1]; v3 = i; v4 = j; v5 = acc
// v5 = 0; v3 = 0;
// while (v3 < ((v1 - v2) + 1)) {
//   v4 = 0;
//   while (v4 < v2) {
//     if (arr[(1+v3)+v4] == arr[(v1+2)+v4]) { v5 = 1; }
//     v4 = v4 + 1;
//   }
//   v3 = v3 + 1;
// }
// arr[0] = v5;
Program buildCatalogOnlyComposite() {
    auto root = makeBlock();
    root->body.push_back(makeAssign(1, makeArrayAccess(makeIntConst(0))));
    root->body.push_back(makeAssign(2, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1)))));
    root->body.push_back(makeAssign(5, makeIntConst(0)));
    root->body.push_back(makeAssign(3, makeIntConst(0)));

    std::vector<StmtPtr> innerBody;
    ExprPtr textIdx = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(3)), makeVar(4));
    ExprPtr patIdx = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(2)), makeVar(4));
    std::vector<StmtPtr> matchBody;
    matchBody.push_back(makeAssign(5, makeIntConst(1)));
    innerBody.push_back(
        makeIf(makeCompare(ExprKind::Eq, makeArrayAccess(std::move(textIdx)), makeArrayAccess(std::move(patIdx))),
              std::move(matchBody)));
    innerBody.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(4), makeIntConst(1))));

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(4, makeIntConst(0)));
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(4), makeVar(2)), std::move(innerBody)));
    outerBody.push_back(makeAssign(3, makeBinOp(ExprKind::Add, makeVar(3), makeIntConst(1))));

    auto outerBound = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Sub, makeVar(1), makeVar(2)), makeIntConst(1));
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(3), std::move(outerBound)), std::move(outerBody)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(5)));
    return root;
}

// Version 2: the ACTUALLY correct naive substring algorithm. Needs a piece
// the catalog does not have: a per-outer-iteration flag that starts true,
// gets cleared on any inner mismatch (AND across the whole window), then is
// compared and OR'd into the accumulator only after the inner loop closes.
// The catalog's relation piece is a single flat statement fired unconditionally
// inside the inner loop; this needs reset-before/compare-after structure
// around the inner loop, which is a materially different (compound) piece.
//
// v1=n v2=m v3=acc v4=i v5=ok v6=j (needs numVars=7 for v0..v6)
Program buildTrueSubstringAlgorithm() {
    auto root = makeBlock();
    root->body.push_back(makeAssign(1, makeArrayAccess(makeIntConst(0))));
    root->body.push_back(makeAssign(2, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1)))));
    root->body.push_back(makeAssign(3, makeIntConst(0)));
    root->body.push_back(makeAssign(4, makeIntConst(0)));

    std::vector<StmtPtr> innerBody;
    ExprPtr textIdx = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(4)), makeVar(6));
    ExprPtr patIdx = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(2)), makeVar(6));
    std::vector<StmtPtr> mismatchBody;
    mismatchBody.push_back(makeAssign(5, makeIntConst(0)));
    innerBody.push_back(
        makeIf(makeCompare(ExprKind::Ne, makeArrayAccess(std::move(textIdx)), makeArrayAccess(std::move(patIdx))),
              std::move(mismatchBody)));
    innerBody.push_back(makeAssign(6, makeBinOp(ExprKind::Add, makeVar(6), makeIntConst(1))));

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(5, makeIntConst(1)));  // ok = 1, reset per i
    outerBody.push_back(makeAssign(6, makeIntConst(0)));
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(6), makeVar(2)), std::move(innerBody)));
    std::vector<StmtPtr> okBody;
    okBody.push_back(makeAssign(3, makeIntConst(1)));
    outerBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(5), makeIntConst(1)), std::move(okBody)));
    outerBody.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(4), makeIntConst(1))));

    auto outerBound = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Sub, makeVar(1), makeVar(2)), makeIntConst(1));
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(4), std::move(outerBound)), std::move(outerBody)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(3)));
    return root;
}

}  // namespace

TEST(catalog_only_composite_does_not_generalize_even_when_clean) {
    // Isolation result: even hand-written with NO growth corruption, the
    // composite catalog's flat "align-match sentinel" (an OR over every
    // (i, j) pair, no per-i reset) is a WRONG algorithm, not merely an
    // un-converged one -- it must fail on some instance by level 3, the
    // level where a text contains a value that spuriously aligns with the
    // pattern at some offset without the full window matching.
    Program composite = buildCatalogOnlyComposite();
    Interpreter interp(100000);
    SubstringProblem problem;

    bool sawFailure = false;
    for (int level = 1; level <= 4 && !sawFailure; ++level) {
        for (const auto& inst : problem.generateInstances(level)) {
            if (problem.evaluate(*composite, inst, interp).status != ExecStatus::SUCCESS) {
                sawFailure = true;
                break;
            }
        }
    }
    CHECK(sawFailure);
}

TEST(true_substring_algorithm_generalizes_and_is_reachable_by_the_interpreter) {
    // The algorithm that IS correct (reset-then-AND-then-OR) sits within the
    // interpreter's expressive power and generalizes across levels 1-8,
    // exact and adversarial instances alike -- confirming the shape is
    // reachable in principle. The gap is specifically that the composite
    // catalog (section 24/27) has no piece expressing this reset+conjunction
    // structure; its single flat relation piece cannot compose it.
    Program correct = buildTrueSubstringAlgorithm();
    Interpreter interp(100000);
    SubstringProblem problem(5, 30, /*numVars=*/7, 42);

    for (int level = 1; level <= 8; ++level) {
        for (const auto& inst : problem.generateInstances(level)) {
            CHECK(problem.evaluate(*correct, inst, interp).status == ExecStatus::SUCCESS);
        }
        for (const auto& inst : problem.generateAdversarialInstances(level)) {
            CHECK(problem.evaluate(*correct, inst, interp).status == ExecStatus::SUCCESS);
        }
    }
}
