#include "ast/Stmt.hpp"
#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/Psb1Problem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(psb1_references_match_hand_computed_values) {
    using K = Psb1Problem::Kind;
    CHECK_EQ(Psb1Problem::reference(K::LastIndexOfZero, {0, 3, 0, 5})[0], 2);
    CHECK_EQ(Psb1Problem::reference(K::CountOdds, {-3, 2, 5, 7, 8})[0], 3);
    auto neg = Psb1Problem::reference(K::NegativeToZero, {-3, 2, -1});
    CHECK_EQ(neg[0], 0);
    CHECK_EQ(neg[1], 2);
    CHECK_EQ(neg[2], 0);
    auto sum = Psb1Problem::reference(K::VectorsSummed, {1, 2, 10, 20});
    CHECK_EQ(sum.size(), static_cast<size_t>(2));
    CHECK_EQ(sum[1], 22);
    CHECK_EQ(Psb1Problem::reference(K::SumOfSquares, {3})[0], 14);
    CHECK_EQ(Psb1Problem::reference(K::CollatzSteps, {6})[0], 8);
    CHECK_EQ(Psb1Problem::reference(K::SmallOrLarge, {999})[0], 1);
    CHECK_EQ(Psb1Problem::reference(K::SmallOrLarge, {1500})[0], 0);
    CHECK_EQ(Psb1Problem::reference(K::SmallOrLarge, {2000})[0], 2);
    CHECK_EQ(Psb1Problem::reference(K::MirrorImage, {1, 2, 2, 1})[0], 1);
    CHECK_EQ(Psb1Problem::reference(K::MirrorImage, {1, 2, 1, 2})[0], 0);
    auto d = Psb1Problem::reference(K::Digits, {120, 0, 0});
    CHECK_EQ(d.size(), static_cast<size_t>(3));
    CHECK_EQ(d[0], 0);
    CHECK_EQ(d[2], 1);
    CHECK_EQ(Psb1Problem::reference(K::Grade, {90, 80, 70, 60, 75})[0], 2);
}

TEST(psb1_problem_builds_train_and_test_sets_that_its_references_solve) {
    using K = Psb1Problem::Kind;
    for (K k : {K::LastIndexOfZero, K::CountOdds, K::NegativeToZero, K::VectorsSummed, K::SumOfSquares, K::CollatzSteps,
                K::SmallOrLarge, K::MirrorImage, K::Digits, K::Grade}) {
        Psb1Problem p(k, 7, 1);
        CHECK_EQ(p.generateInstances(1).size(), static_cast<size_t>(200));
        CHECK_EQ(p.generateAdversarialInstances(1).size(), static_cast<size_t>(2000));
        for (const auto& inst : p.generateInstances(1)) CHECK(p.isSolution(inst.input, inst.expectedOutput));
    }
}

TEST(psb1_medium_version_uses_small_ranges_and_scaled_thresholds) {
    using K = Psb1Problem::Kind;
    CHECK_EQ(Psb1Problem::reference(K::SmallOrLarge, {9}, true)[0], 1);
    CHECK_EQ(Psb1Problem::reference(K::SmallOrLarge, {15}, true)[0], 0);
    CHECK_EQ(Psb1Problem::reference(K::SmallOrLarge, {20}, true)[0], 2);
    Psb1Problem p(K::CountOdds, 7, 1, true);
    for (const auto& inst : p.generateAdversarialInstances(1)) {
        CHECK(inst.input.size() <= static_cast<size_t>(8));
        for (int x : inst.input) CHECK(x >= -20 && x <= 20);
        CHECK(p.isSolution(inst.input, inst.expectedOutput));
    }
}

namespace {
int psb1FailuresOn(Psb1Problem& prob, const Stmt& program) {
    Interpreter interp(20000);
    int bad = 0;
    for (const auto& inst : prob.generateAdversarialInstances(1)) {
        if (prob.evaluate(program, inst, interp).status != ExecStatus::SUCCESS) ++bad;
    }
    return bad;
}
}  // namespace

TEST(psb1_medium_count_odds_and_sum_of_squares_are_expressible_in_the_grammar) {
    // count_odds: v6 = 0; v4 = 0; while (v4 < v0) { if (arr[v4] % 2 != 0) { v6 = v6 + 1; } v4 = v4 + 1; } arr[0] = v6;
    auto a = makeBlock();
    a->body.push_back(makeAssign(6, makeIntConst(0)));
    a->body.push_back(makeAssign(4, makeIntConst(0)));
    std::vector<StmtPtr> hit;
    hit.push_back(makeAssign(6, makeBinOp(ExprKind::Add, makeVar(6), makeIntConst(1))));
    std::vector<StmtPtr> body;
    body.push_back(makeIf(makeCompare(ExprKind::Ne, makeBinOp(ExprKind::Mod, makeArrayAccess(makeVar(4)), makeIntConst(2)),
                                      makeIntConst(0)),
                          std::move(hit)));
    body.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(4), makeIntConst(1))));
    a->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(4), makeVar(0)), std::move(body)));
    a->body.push_back(makeAssignArray(makeIntConst(0), makeVar(6)));
    Psb1Problem odds(Psb1Problem::Kind::CountOdds, 7, 1, true);
    CHECK_EQ(psb1FailuresOn(odds, *a), 0);

    // sum_of_squares: v6 = 0; v4 = 1; while (v4 <= arr[0]) { v6 = v6 + v4 * v4; v4 = v4 + 1; } arr[0] = v6;
    auto b = makeBlock();
    b->body.push_back(makeAssign(6, makeIntConst(0)));
    b->body.push_back(makeAssign(4, makeIntConst(1)));
    std::vector<StmtPtr> body2;
    body2.push_back(makeAssign(6, makeBinOp(ExprKind::Add, makeVar(6), makeBinOp(ExprKind::Mul, makeVar(4), makeVar(4)))));
    body2.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(4), makeIntConst(1))));
    b->body.push_back(makeWhile(makeCompare(ExprKind::Le, makeVar(4), makeArrayAccess(makeIntConst(0))), std::move(body2)));
    b->body.push_back(makeAssignArray(makeIntConst(0), makeVar(6)));
    Psb1Problem squares(Psb1Problem::Kind::SumOfSquares, 7, 1, true);
    CHECK_EQ(psb1FailuresOn(squares, *b), 0);
}

TEST(psb1_section_137_references_match_hand_computed_values) {
    using K = Psb1Problem::Kind;
    CHECK_EQ(Psb1Problem::reference(K::CountMultiplesOfThree, {-3, 0, 4, 9, 10})[0], 3);
    CHECK_EQ(Psb1Problem::reference(K::EvenSquares, {50})[0], 3);  // 4, 16, 36
    CHECK_EQ(Psb1Problem::reference(K::SumOfCubes, {3})[0], 36);
    CHECK_EQ(Psb1Problem::reference(K::ForLoopIndex, {2, 10, 3})[0], 2 + 5 + 8);
    CHECK_EQ(Psb1Problem::reference(K::Smallest4, {5, -2, 7, 0})[0], -2);
    for (K k : {K::CountMultiplesOfThree, K::EvenSquares, K::SumOfCubes, K::ForLoopIndex, K::Smallest4}) {
        Psb1Problem p(k, 7, 1, true);
        for (const auto& inst : p.generateInstances(1)) CHECK(p.isSolution(inst.input, inst.expectedOutput));
    }
}
