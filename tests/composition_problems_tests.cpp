#include <set>
#include <string>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/CountAscendingRunsProblem.hpp"
#include "problems/CountDistinctProblem.hpp"
#include "problems/CountRunsProblem.hpp"
#include "problems/CountLeadersProblem.hpp"
#include "problems/HasMajorityProblem.hpp"
#include "problems/LongestIncreasingRunProblem.hpp"
#include "problems/MaxGapSortedProblem.hpp"
#include "problems/MaxProfitProblem.hpp"
#include "problems/MaxRiseProblem.hpp"
#include "problems/MaxSubarraySumProblem.hpp"
#include "problems/MedianProblem.hpp"
#include "problems/MinGapProblem.hpp"
#include "problems/MinStepProblem.hpp"
#include "problems/SecondSmallestProblem.hpp"
#include "problems/TrappingRainWaterProblem.hpp"
#include "problems/CeilingWaterProblem.hpp"
#include "problems/LongestDecreasingSubsequenceProblem.hpp"
#include "problems/LongestNonIncreasingRunProblem.hpp"
#include "problems/MaxSumIncreasingSubsequenceProblem.hpp"
#include "problems/MinSubarraySumProblem.hpp"
#include "problems/CountDominatedProblem.hpp"
#include "problems/CountUniqueProblem.hpp"
#include "problems/MaxDrawdownProblem.hpp"
#include "problems/MaxFrequencyProblem.hpp"
#include "problems/TotalVariationProblem.hpp"
#include "problems/ElementsTwiceProblem.hpp"
#include "problems/MinFrequencyProblem.hpp"
#include "problems/ModeProblem.hpp"
#include "problems/ValuesTwiceProblem.hpp"
#include "problems/FirstMinusLastProblem.hpp"
#include "problems/LargestDropStepProblem.hpp"
#include "problems/SumAbsFromFirstProblem.hpp"
#include "problems/SumOfRisesProblem.hpp"
#include "problems/ArgmaxIndexProblem.hpp"
#include "problems/LeastFrequentValueProblem.hpp"
#include "problems/LongestRunValueProblem.hpp"
#include "problems/MajorityValueProblem.hpp"
#include "problems/EqualPairsProblem.hpp"
#include "problems/RareValuesProblem.hpp"
#include "problems/RepeatedValuesProblem.hpp"
#include "problems/SumDistinctProblem.hpp"
#include "problems/FrequentValuesProblem.hpp"
#include "problems/HasDoubleValueProblem.hpp"
#include "problems/OddFrequencyValuesProblem.hpp"
#include "search/Benchmark.hpp"
#include "test_framework.hpp"

using namespace algosearch;

// FINDINGS.md section 50's post-freeze problems: each gets its labels checked
// against hand-computed cases and a hand-written Mini-C reference program that
// must pass every generated, adversarial and exhaustive instance -- so every
// problem is known to be solvable in the language before any search runs.
// Conventions: v0 = n; v1, v2 loop indices; v3 result (written to arr[0]);
// v4-v7 scratch.

namespace {

ExprPtr var(int v) { return makeVar(v); }
ExprPtr lit(int c) { return makeIntConst(c); }
ExprPtr add(ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Add, std::move(a), std::move(b)); }
ExprPtr sub(ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Sub, std::move(a), std::move(b)); }
ExprPtr cmp(ExprKind k, ExprPtr a, ExprPtr b) { return makeCompare(k, std::move(a), std::move(b)); }
ExprPtr at(ExprPtr i) { return makeArrayAccess(std::move(i)); }
StmtPtr inc(int v) { return makeAssign(v, add(var(v), lit(1))); }

template <typename... S>
std::vector<StmtPtr> stmts(S... s) {
    std::vector<StmtPtr> v;
    (v.push_back(std::move(s)), ...);
    return v;
}

// Selection sort of the whole array, in place.
void appendSort(Stmt& p) {
    auto inner = stmts(makeIf(cmp(ExprKind::Lt, at(var(2)), at(var(4))), stmts(makeAssign(4, var(2)))), inc(2));
    auto outer = stmts(makeAssign(4, var(1)), makeAssign(2, add(var(1), lit(1))),
                       makeWhile(cmp(ExprKind::Lt, var(2), var(0)), std::move(inner)),
                       makeSwap(var(1), var(4)), inc(1));
    p.body.push_back(makeAssign(1, lit(0)));
    p.body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
}

// v1 = 1; while (v1 < n) { body; v1++ }  -- prev = arr[v1-1], cur = arr[v1].
void appendAdjacentPass(Stmt& p, std::vector<StmtPtr> body) {
    body.push_back(inc(1));
    p.body.push_back(makeAssign(1, lit(1)));
    p.body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(body)));
}
ExprPtr prev() { return at(sub(var(1), lit(1))); }
ExprPtr cur() { return at(var(1)); }

Program finish(StmtPtr p) {
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    return p;
}

// `minVariedN`: smallest n whose labels must not all be equal (rain water
// needs 3 bars to hold any water).
void checkProblem(const ScalarFromArrayProblem& problem, const Stmt& reference, int minVariedN = 2) {
    Interpreter interp(1000000);
    for (int n = 1; n <= 8; ++n) {
        std::set<int> labels;
        auto instances = problem.generateInstances(n);
        CHECK(!instances.empty());
        for (const auto& inst : instances) {
            CHECK_EQ(inst.expectedOutput[0], problem.answer(inst.input));
            labels.insert(inst.expectedOutput[0]);
            CHECK(problem.evaluate(reference, inst, interp).status == ExecStatus::SUCCESS);
        }
        if (n >= minVariedN) CHECK(labels.size() >= 2);  // no single-answer shortcut
        for (const auto& inst : problem.generateAdversarialInstances(n)) {
            CHECK(problem.evaluate(reference, inst, interp).status == ExecStatus::SUCCESS);
        }
    }
    std::string reason;
    CHECK(verifyGeneralization(problem, reference, 10000000, reason, false));  // includes the exhaustive check
}

// Sort, then v3 = arr[index].
Program sortThenRead(ExprPtr index) {
    auto p = makeBlock();
    appendSort(*p);
    p->body.push_back(makeAssign(3, at(std::move(index))));
    return finish(std::move(p));
}

// Sort, then an adjacent pass with v5 = cur - prev and `update` on it.
Program sortThenGap(StmtPtr init, StmtPtr update) {
    auto p = makeBlock();
    appendSort(*p);
    p->body.push_back(std::move(init));
    appendAdjacentPass(*p, stmts(makeAssign(5, sub(cur(), prev())), std::move(update)));
    return finish(std::move(p));
}

}  // namespace

TEST(median_problem_labels_and_reference_solution) {
    MedianProblem problem;
    CHECK_EQ(problem.answer({5, 1, 3}), 3);
    CHECK_EQ(problem.answer({4, 1, 3, 2}), 2);  // lower median
    checkProblem(problem, *sortThenRead(makeBinOp(ExprKind::Div, sub(var(0), lit(1)), lit(2))));
}

TEST(second_smallest_problem_labels_and_reference_solution) {
    SecondSmallestProblem problem;
    CHECK_EQ(problem.answer({5, 1, 3}), 3);
    CHECK_EQ(problem.answer({1, 1, 3}), 1);  // duplicates count
    CHECK_EQ(problem.answer({7}), 7);
    auto p = makeBlock();
    appendSort(*p);
    p->body.push_back(makeAssign(3, at(lit(0))));
    p->body.push_back(makeIf(cmp(ExprKind::Lt, lit(1), var(0)), stmts(makeAssign(3, at(lit(1))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(count_distinct_problem_labels_and_reference_solution) {
    CountDistinctProblem problem;
    CHECK_EQ(problem.answer({2, 1, 2, 3, 1}), 3);
    CHECK_EQ(problem.answer({4}), 1);
    auto p = makeBlock();
    appendSort(*p);
    p->body.push_back(makeAssign(3, lit(1)));
    appendAdjacentPass(*p, stmts(makeIf(cmp(ExprKind::Ne, prev(), cur()), stmts(inc(3)))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(min_gap_problem_labels_and_reference_solution) {
    MinGapProblem problem;
    CHECK_EQ(problem.answer({10, 1, 4, 6}), 2);
    CHECK_EQ(problem.answer({3, 3}), 0);
    CHECK_EQ(problem.answer({7}), 0);
    // v3 starts at the first gap (or 0 when n < 2), then keeps the minimum.
    auto p = makeBlock();
    appendSort(*p);
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(
        makeIf(cmp(ExprKind::Lt, lit(1), var(0)), stmts(makeAssign(3, sub(at(lit(1)), at(lit(0)))))));
    appendAdjacentPass(*p, stmts(makeAssign(5, sub(cur(), prev())),
                                 makeIf(cmp(ExprKind::Lt, var(5), var(3)), stmts(makeAssign(3, var(5))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(max_gap_sorted_problem_labels_and_reference_solution) {
    MaxGapSortedProblem problem;
    CHECK_EQ(problem.answer({10, 1, 4, 6}), 4);
    CHECK_EQ(problem.answer({7}), 0);
    checkProblem(problem, *sortThenGap(makeAssign(3, lit(0)),
                                       makeIf(cmp(ExprKind::Gt, var(5), var(3)), stmts(makeAssign(3, var(5))))));
}

TEST(max_subarray_sum_problem_labels_and_reference_solution) {
    MaxSubarraySumProblem problem;
    CHECK_EQ(problem.answer({-2, 1, -3, 4, -1, 2, 1, -5, 4}), 6);
    CHECK_EQ(problem.answer({-3, -1, -2}), -1);
    // Kadane: v4 = best sum ending here, v3 = best overall.
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, at(lit(0))));
    p->body.push_back(makeAssign(4, at(lit(0))));
    appendAdjacentPass(*p, stmts(makeIf(cmp(ExprKind::Lt, var(4), lit(0)), stmts(makeAssign(4, cur())),
                                        stmts(makeAssign(4, add(var(4), cur())))),
                                 makeIf(cmp(ExprKind::Gt, var(4), var(3)), stmts(makeAssign(3, var(4))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(longest_increasing_run_problem_labels_and_reference_solution) {
    LongestIncreasingRunProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 4, 1}), 3);
    CHECK_EQ(problem.answer({4}), 1);
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(1)));
    p->body.push_back(makeAssign(4, lit(1)));
    appendAdjacentPass(*p, stmts(makeIf(cmp(ExprKind::Lt, prev(), cur()), stmts(inc(4)), stmts(makeAssign(4, lit(1)))),
                                 makeIf(cmp(ExprKind::Gt, var(4), var(3)), stmts(makeAssign(3, var(4))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(max_profit_problem_labels_and_reference_solution) {
    MaxProfitProblem problem;
    CHECK_EQ(problem.answer({7, 1, 5, 3, 6, 4}), 5);
    CHECK_EQ(problem.answer({7, 6, 4, 3, 1}), 0);
    // Running minimum v4, best profit v3.
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(4, at(lit(0))));
    appendAdjacentPass(*p, stmts(makeIf(cmp(ExprKind::Gt, sub(cur(), var(4)), var(3)),
                                        stmts(makeAssign(3, sub(cur(), var(4))))),
                                 makeIf(cmp(ExprKind::Lt, cur(), var(4)), stmts(makeAssign(4, cur())))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(count_leaders_problem_labels_and_reference_solution) {
    CountLeadersProblem problem;
    CHECK_EQ(problem.answer({16, 17, 4, 3, 5, 2}), 3);  // 17, 5, 2
    CHECK_EQ(problem.answer({2, 2}), 1);                // strictly greater
    // Right to left with a running maximum v5.
    auto p = makeBlock();
    p->body.push_back(makeAssign(1, sub(var(0), lit(1))));
    p->body.push_back(makeAssign(5, at(var(1))));
    p->body.push_back(makeAssign(3, lit(1)));
    p->body.push_back(makeAssign(1, sub(var(1), lit(1))));
    p->body.push_back(makeWhile(cmp(ExprKind::Ge, var(1), lit(0)),
                                stmts(makeIf(cmp(ExprKind::Gt, at(var(1)), var(5)),
                                             stmts(inc(3), makeAssign(5, at(var(1))))),
                                      makeAssign(1, sub(var(1), lit(1))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(has_majority_problem_labels_and_reference_solution) {
    HasMajorityProblem problem;
    CHECK_EQ(problem.answer({2, 1, 2}), 1);
    CHECK_EQ(problem.answer({2, 1, 2, 1}), 0);  // exactly half is not a majority
    // For each i, count equal elements; majority when 2 * count > n.
    auto inner = stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(5))), inc(2));
    auto outer = stmts(makeAssign(5, lit(0)), makeAssign(2, lit(0)),
                       makeWhile(cmp(ExprKind::Lt, var(2), var(0)), std::move(inner)),
                       makeIf(cmp(ExprKind::Gt, add(var(5), var(5)), var(0)), stmts(makeAssign(3, lit(1)))), inc(1));
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(trapping_rain_water_problem_labels_and_reference_solution) {
    TrappingRainWaterProblem problem;
    CHECK_EQ(problem.answer({0, 1, 0, 2, 1, 0, 1, 3, 2, 1, 2, 1}), 6);
    CHECK_EQ(problem.answer({1, 2, 3}), 0);
    // For each i: v4 = max of arr[0..i], v5 = max of arr[i..n), add min(v4, v5) - arr[i].
    auto left = stmts(makeIf(cmp(ExprKind::Gt, at(var(2)), var(4)), stmts(makeAssign(4, at(var(2))))), inc(2));
    auto right = stmts(makeIf(cmp(ExprKind::Gt, at(var(2)), var(5)), stmts(makeAssign(5, at(var(2))))), inc(2));
    auto outer = stmts(makeAssign(4, at(var(1))), makeAssign(2, lit(0)),
                       makeWhile(cmp(ExprKind::Le, var(2), var(1)), std::move(left)), makeAssign(5, at(var(1))),
                       makeAssign(2, var(1)), makeWhile(cmp(ExprKind::Lt, var(2), var(0)), std::move(right)),
                       makeIf(cmp(ExprKind::Lt, var(5), var(4)), stmts(makeAssign(4, var(5)))),
                       makeAssign(3, add(var(3), sub(var(4), at(var(1))))), inc(1));
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    checkProblem(problem, *finish(std::move(p)), /*minVariedN=*/3);
}

// Section 52's post-freeze problems: one adjacent pass each.

namespace {

// v3 = first step (0 when n < 2), then the running best of the steps.
Program stepPass(ExprKind keep) {
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeIf(cmp(ExprKind::Lt, lit(1), var(0)), stmts(makeAssign(3, sub(at(lit(1)), at(lit(0)))))));
    appendAdjacentPass(*p, stmts(makeAssign(5, sub(cur(), prev())),
                                 makeIf(cmp(keep, var(5), var(3)), stmts(makeAssign(3, var(5))))));
    return finish(std::move(p));
}

// v3 = 1 + the number of adjacent pairs with `boundary(prev, cur)`.
Program segmentPass(ExprKind boundary) {
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(1)));
    appendAdjacentPass(*p, stmts(makeIf(cmp(boundary, prev(), cur()), stmts(inc(3)))));
    return finish(std::move(p));
}

}  // namespace

TEST(max_rise_problem_labels_and_reference_solution) {
    MaxRiseProblem problem;
    CHECK_EQ(problem.answer({1, 4, 2, 3}), 3);
    CHECK_EQ(problem.answer({5, 3, 0}), -2);  // only falls
    CHECK_EQ(problem.answer({7}), 0);
    checkProblem(problem, *stepPass(ExprKind::Gt));
}

TEST(min_step_problem_labels_and_reference_solution) {
    MinStepProblem problem;
    CHECK_EQ(problem.answer({1, 4, 2, 3}), -2);
    CHECK_EQ(problem.answer({7}), 0);
    checkProblem(problem, *stepPass(ExprKind::Lt));
}

TEST(count_runs_problem_labels_and_reference_solution) {
    CountRunsProblem problem;
    CHECK_EQ(problem.answer({1, 1, 2, 2, 1}), 3);
    CHECK_EQ(problem.answer({4}), 1);
    checkProblem(problem, *segmentPass(ExprKind::Ne));
}

TEST(count_ascending_runs_problem_labels_and_reference_solution) {
    CountAscendingRunsProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 1}), 3);  // [1 2] [2 3] [1]
    CHECK_EQ(problem.answer({4}), 1);
    checkProblem(problem, *segmentPass(ExprKind::Ge));
}

// Section 55's post-freeze problems.

namespace {

ExprPtr auxAt(ExprPtr i) { return makeAuxAccess(std::move(i)); }

// aux[i] = weight + best aux[j] over j < i with keep(arr[j], arr[i]); v3 = best aux[i].
Program earlierCells(ExprKind keep, bool unitWeight) {
    auto inner = stmts(makeIf(cmp(keep, at(var(2)), at(var(1))),
                              stmts(makeIf(cmp(ExprKind::Gt, auxAt(var(2)), var(5)), stmts(makeAssign(5, auxAt(var(2))))))),
                       inc(2));
    auto outer = stmts(makeAssign(5, lit(0)), makeAssign(2, lit(0)),
                       makeWhile(cmp(ExprKind::Lt, var(2), var(1)), std::move(inner)),
                       makeAssignAux(var(1), add(var(5), unitWeight ? lit(1) : at(var(1)))),
                       makeIf(cmp(ExprKind::Gt, auxAt(var(1)), var(3)), stmts(makeAssign(3, auxAt(var(1))))), inc(1));
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    return finish(std::move(p));
}

}  // namespace

TEST(longest_non_increasing_run_problem_labels_and_reference_solution) {
    LongestNonIncreasingRunProblem problem;
    CHECK_EQ(problem.answer({3, 3, 1, 2, 2, 0}), 3);
    CHECK_EQ(problem.answer({4}), 1);
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(1)));
    p->body.push_back(makeAssign(4, lit(1)));
    appendAdjacentPass(*p, stmts(makeIf(cmp(ExprKind::Ge, prev(), cur()), stmts(inc(4)), stmts(makeAssign(4, lit(1)))),
                                 makeIf(cmp(ExprKind::Gt, var(4), var(3)), stmts(makeAssign(3, var(4))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(min_subarray_sum_problem_labels_and_reference_solution) {
    MinSubarraySumProblem problem;
    CHECK_EQ(problem.answer({3, -4, 2, -3, -1, 7, -5}), -6);
    CHECK_EQ(problem.answer({2, 1, 3}), 1);
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, at(lit(0))));
    p->body.push_back(makeAssign(4, at(lit(0))));
    appendAdjacentPass(*p, stmts(makeIf(cmp(ExprKind::Gt, var(4), lit(0)), stmts(makeAssign(4, cur())),
                                        stmts(makeAssign(4, add(var(4), cur())))),
                                 makeIf(cmp(ExprKind::Lt, var(4), var(3)), stmts(makeAssign(3, var(4))))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(longest_decreasing_subsequence_problem_labels_and_reference_solution) {
    LongestDecreasingSubsequenceProblem problem;
    CHECK_EQ(problem.answer({5, 1, 4, 3, 2}), 4);
    CHECK_EQ(problem.answer({1, 1}), 1);
    checkProblem(problem, *earlierCells(ExprKind::Gt, /*unitWeight=*/true));
}

TEST(max_sum_increasing_subsequence_problem_labels_and_reference_solution) {
    MaxSumIncreasingSubsequenceProblem problem;
    CHECK_EQ(problem.answer({1, 101, 2, 3, 100, 4, 5}), 106);
    CHECK_EQ(problem.answer({-3, -1}), 0);  // empty subsequence
    checkProblem(problem, *earlierCells(ExprKind::Lt, /*unitWeight=*/false));
}

TEST(ceiling_water_problem_labels_and_reference_solution) {
    CeilingWaterProblem problem;
    CHECK_EQ(problem.answer({0, 3, 1, 4, 0}), 8);  // 3 + 1 + 4 hang below the ceiling at 0
    CHECK_EQ(problem.answer({3, 2, 1}), 0);
    // For each i: v4 = min of arr[0..i], v5 = min of arr[i..n), add arr[i] - max(v4, v5).
    auto left = stmts(makeIf(cmp(ExprKind::Lt, at(var(2)), var(4)), stmts(makeAssign(4, at(var(2))))), inc(2));
    auto right = stmts(makeIf(cmp(ExprKind::Lt, at(var(2)), var(5)), stmts(makeAssign(5, at(var(2))))), inc(2));
    auto outer = stmts(makeAssign(4, at(var(1))), makeAssign(2, lit(0)),
                       makeWhile(cmp(ExprKind::Le, var(2), var(1)), std::move(left)), makeAssign(5, at(var(1))),
                       makeAssign(2, var(1)), makeWhile(cmp(ExprKind::Lt, var(2), var(0)), std::move(right)),
                       makeIf(cmp(ExprKind::Gt, var(5), var(4)), stmts(makeAssign(4, var(5)))),
                       makeAssign(3, add(var(3), sub(at(var(1)), var(4)))), inc(1));
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    checkProblem(problem, *finish(std::move(p)), /*minVariedN=*/3);
}

// -- Section 71 post-freeze problems --

namespace {

// v1 = 0; while (v1 < n) { reset; v2 = start; while (v2 < n) { inner; v2++ } consume; v1++ }
// with start = v1 + 1 (triangular) or 0; elem(i) = arr[v1], elem(j) = arr[v2].
Program nestedPass(bool triangular, std::vector<StmtPtr> reset, std::vector<StmtPtr> inner, std::vector<StmtPtr> consume) {
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    inner.push_back(inc(2));
    std::vector<StmtPtr> outer = std::move(reset);
    outer.push_back(makeAssign(2, triangular ? add(var(1), lit(1)) : lit(0)));
    outer.push_back(makeWhile(cmp(ExprKind::Lt, var(2), var(0)), std::move(inner)));
    for (auto& st : consume) outer.push_back(std::move(st));
    outer.push_back(inc(1));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    return finish(std::move(p));
}

}  // namespace

TEST(max_frequency_problem_labels_and_reference_solution) {
    MaxFrequencyProblem problem;
    CHECK_EQ(problem.answer({3, 1, 3, 2, 3}), 3);
    CHECK_EQ(problem.answer({4}), 1);
    checkProblem(problem, *nestedPass(false, stmts(makeAssign(4, lit(0))),
                                      stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(4)))),
                                      stmts(makeIf(cmp(ExprKind::Gt, var(4), var(3)), stmts(makeAssign(3, var(4)))))));
}

TEST(count_unique_problem_labels_and_reference_solution) {
    CountUniqueProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3}), 2);
    CHECK_EQ(problem.answer({5, 5}), 0);
    checkProblem(problem, *nestedPass(false, stmts(makeAssign(4, lit(0))),
                                      stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(4)))),
                                      stmts(makeIf(cmp(ExprKind::Eq, var(4), lit(1)), stmts(inc(3))))));
}

TEST(count_dominated_problem_labels_and_reference_solution) {
    CountDominatedProblem problem;
    CHECK_EQ(problem.answer({1, 3, 2, 2}), 1);  // only the 1 has a greater element later
    CHECK_EQ(problem.answer({4, 3, 3}), 0);
    checkProblem(problem, *nestedPass(true, stmts(makeAssign(4, lit(0))),
                                      stmts(makeIf(cmp(ExprKind::Gt, at(var(2)), at(var(1))), stmts(makeAssign(4, lit(1))))),
                                      stmts(makeIf(cmp(ExprKind::Eq, var(4), lit(1)), stmts(inc(3))))));
}

TEST(max_drawdown_problem_labels_and_reference_solution) {
    MaxDrawdownProblem problem;
    CHECK_EQ(problem.answer({3, 7, 1, 5}), 6);
    CHECK_EQ(problem.answer({1, 2, 3}), 0);
    checkProblem(problem, *nestedPass(true, stmts(),
                                      stmts(makeIf(cmp(ExprKind::Gt, sub(at(var(1)), at(var(2))), var(3)),
                                                   stmts(makeAssign(3, sub(at(var(1)), at(var(2))))))),
                                      stmts()));
}

TEST(total_variation_problem_labels_and_reference_solution) {
    TotalVariationProblem problem;
    CHECK_EQ(problem.answer({1, 4, 2, 2}), 5);
    CHECK_EQ(problem.answer({7}), 0);
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    appendAdjacentPass(*p, stmts(makeAssign(5, sub(cur(), prev())),
                                 makeIf(cmp(ExprKind::Lt, var(5), lit(0)), stmts(makeAssign(5, sub(lit(0), var(5))))),
                                 makeAssign(3, add(var(3), var(5)))));
    checkProblem(problem, *finish(std::move(p)));
}

// -- Section 75 post-freeze problems: frequency table v5 (count of equal
// elements, per outer element v1) feeding an outer test or best, or (mode)
// the argmax value. --

namespace {

Program frequencyPass(std::vector<StmtPtr> consume) {
    return nestedPass(false, stmts(makeAssign(4, lit(0))),
                      stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(4)))), std::move(consume));
}

}  // namespace

TEST(elements_twice_problem_labels_and_reference_solution) {
    ElementsTwiceProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3, 3}), 2);
    checkProblem(problem, *frequencyPass(stmts(makeIf(cmp(ExprKind::Eq, var(4), lit(2)), stmts(inc(3))))));
}

TEST(min_frequency_problem_labels_and_reference_solution) {
    MinFrequencyProblem problem;
    CHECK_EQ(problem.answer({1, 1, 2, 2, 2}), 2);
    CHECK_EQ(problem.answer({}), 0);
    // v3 = 0 means "unset": keep t when v3 is 0 or t < v3.
    checkProblem(problem, *frequencyPass(stmts(makeIf(cmp(ExprKind::Eq, var(3), lit(0)), stmts(makeAssign(3, var(4)))),
                                               makeIf(cmp(ExprKind::Lt, var(4), var(3)), stmts(makeAssign(3, var(4)))))));
}

TEST(mode_problem_labels_and_reference_solution) {
    ModeProblem problem;
    CHECK_EQ(problem.answer({3, 1, 3, 1, 2}), 1);  // tie between 1 and 3: the smaller
    // v6 = best count, v3 = its value; a strictly better count, or a tie with a smaller value.
    auto take = [] { return stmts(makeAssign(6, var(4)), makeAssign(3, at(var(1)))); };
    checkProblem(problem, *frequencyPass(stmts(
                              makeIf(cmp(ExprKind::Gt, var(4), var(6)), take()),
                              makeIf(cmp(ExprKind::Eq, var(4), var(6)),
                                     stmts(makeIf(cmp(ExprKind::Lt, at(var(1)), var(3)), take()))))));
}

TEST(values_twice_problem_labels_and_reference_solution) {
    ValuesTwiceProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3, 4, 4, 4}), 2);
    // Each value occurring twice is counted at both elements: halve at the end.
    auto p = frequencyPass(stmts(makeIf(cmp(ExprKind::Eq, var(4), lit(2)), stmts(inc(3)))));
    p->body.insert(p->body.end() - 1, makeAssign(3, makeBinOp(ExprKind::Div, var(3), lit(2))));
    checkProblem(problem, *p);
}

// -- Section 80 post-freeze problems: adjacent passes over the step v5. --

namespace {

// v3 = init; for each adjacent pair: v5 = step; `use`.
Program adjacentStepPass(std::vector<StmtPtr> init, ExprPtr step, std::vector<StmtPtr> use) {
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    for (auto& st : init) p->body.push_back(std::move(st));
    std::vector<StmtPtr> body;
    body.push_back(makeAssign(5, std::move(step)));
    for (auto& st : use) body.push_back(std::move(st));
    appendAdjacentPass(*p, std::move(body));
    return finish(std::move(p));
}

}  // namespace

TEST(first_minus_last_problem_labels_and_reference_solution) {
    FirstMinusLastProblem problem;
    CHECK_EQ(problem.answer({5, 1, 2}), 3);
    CHECK_EQ(problem.answer({}), 0);
    checkProblem(problem, *adjacentStepPass(stmts(), sub(prev(), cur()), stmts(makeAssign(3, add(var(3), var(5))))));
}

TEST(largest_drop_step_problem_labels_and_reference_solution) {
    LargestDropStepProblem problem;
    CHECK_EQ(problem.answer({5, 1, 2}), 4);
    CHECK_EQ(problem.answer({1, 2, 4}), -1);  // every step rises: the least negative
    CHECK_EQ(problem.answer({7}), 0);
    checkProblem(problem, *adjacentStepPass(stmts(makeIf(cmp(ExprKind::Lt, lit(1), var(0)), stmts(makeAssign(3, sub(at(lit(0)), at(lit(1))))))),
                                            sub(prev(), cur()),
                                            stmts(makeIf(cmp(ExprKind::Gt, var(5), var(3)), stmts(makeAssign(3, var(5)))))));
}

TEST(sum_of_rises_problem_labels_and_reference_solution) {
    SumOfRisesProblem problem;
    CHECK_EQ(problem.answer({1, 4, 2, 3}), 4);
    checkProblem(problem, *adjacentStepPass(stmts(), sub(cur(), prev()),
                                            stmts(makeIf(cmp(ExprKind::Gt, var(5), lit(0)), stmts(makeAssign(3, add(var(3), var(5))))))));
}

TEST(sum_abs_from_first_problem_labels_and_reference_solution) {
    SumAbsFromFirstProblem problem;
    CHECK_EQ(problem.answer({2, 5, 0}), 5);
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)),
                                stmts(makeAssign(5, sub(at(var(1)), at(lit(0)))),
                                      makeIf(cmp(ExprKind::Lt, var(5), lit(0)), stmts(makeAssign(5, sub(lit(0), var(5))))),
                                      makeAssign(3, add(var(3), var(5))), inc(1))));
    checkProblem(problem, *finish(std::move(p)));
}

// -- Section 82 post-freeze problems --

TEST(argmax_index_problem_labels_and_reference_solution) {
    ArgmaxIndexProblem problem;
    CHECK_EQ(problem.answer({3, 9, 2, 9}), 1);
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(1, lit(1)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)),
                                stmts(makeIf(cmp(ExprKind::Gt, at(var(1)), at(var(3))), stmts(makeAssign(3, var(1)))), inc(1))));
    checkProblem(problem, *finish(std::move(p)));
}

TEST(least_frequent_value_problem_labels_and_reference_solution) {
    LeastFrequentValueProblem problem;
    CHECK_EQ(problem.answer({2, 2, 1, 3, 3}), 1);
    CHECK_EQ(problem.answer({4, 1}), 1);  // tie: the smaller
    // v6 = best count (0 = unset), v3 = its value.
    auto take = [] { return stmts(makeAssign(6, var(4)), makeAssign(3, at(var(1)))); };
    checkProblem(problem, *nestedPass(false, stmts(makeAssign(4, lit(0))),
                                      stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(4)))),
                                      stmts(makeIf(cmp(ExprKind::Eq, var(6), lit(0)), take()),
                                            makeIf(cmp(ExprKind::Lt, var(4), var(6)), take()),
                                            makeIf(cmp(ExprKind::Eq, var(4), var(6)),
                                                   stmts(makeIf(cmp(ExprKind::Lt, at(var(1)), var(3)), take()))))));
}

TEST(majority_value_problem_labels_and_reference_solution) {
    MajorityValueProblem problem;
    CHECK_EQ(problem.answer({2, 1, 2}), 2);
    CHECK_EQ(problem.answer({1, 2}), -1);
    // v3 = -1; if count(arr[i]) * 2 > n then v3 = arr[i].
    auto p = nestedPass(false, stmts(makeAssign(4, lit(0))),
                        stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(4)))),
                        stmts(makeIf(cmp(ExprKind::Gt, add(var(4), var(4)), var(0)), stmts(makeAssign(3, at(var(1)))))));
    p->body[0] = makeAssign(3, lit(-1));
    checkProblem(problem, *p);
}

TEST(longest_run_value_problem_labels_and_reference_solution) {
    LongestRunValueProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3}), 2);
    CHECK_EQ(problem.answer({}), 0);
    // v4 = current run, v6 = best run length, v3 = its value.
    auto p = makeBlock();
    p->body.push_back(makeAssign(3, lit(0)));
    p->body.push_back(makeAssign(6, lit(0)));
    p->body.push_back(makeAssign(4, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    std::vector<StmtPtr> body;
    body.push_back(makeAssign(4, add(var(4), lit(1))));
    body.push_back(makeIf(cmp(ExprKind::Lt, lit(0), var(1)),
                          stmts(makeIf(cmp(ExprKind::Ne, at(var(1)), at(sub(var(1), lit(1)))), stmts(makeAssign(4, lit(1)))))));
    body.push_back(makeIf(cmp(ExprKind::Gt, var(4), var(6)), stmts(makeAssign(6, var(4)), makeAssign(3, at(var(1))))));
    body.push_back(inc(1));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(body)));
    checkProblem(problem, *finish(std::move(p)));
}

// -- Section 86 post-freeze problems: frequency v4 and an earlier-equal flag v6
// per outer element v1; the outer step only sees first occurrences. --

namespace {

Program firstOccurrencePass(std::vector<StmtPtr> consume) {
    std::vector<StmtPtr> guarded;
    guarded.push_back(makeIf(cmp(ExprKind::Eq, var(6), lit(0)), std::move(consume)));
    return nestedPass(false, stmts(makeAssign(4, lit(0)), makeAssign(6, lit(0))),
                      stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(4))),
                            makeIf(cmp(ExprKind::Lt, var(2), var(1)),
                                   stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(makeAssign(6, lit(1))))))),
                      std::move(guarded));
}

}  // namespace

TEST(repeated_values_problem_labels_and_reference_solution) {
    RepeatedValuesProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3, 3}), 2);
    checkProblem(problem, *firstOccurrencePass(stmts(makeIf(cmp(ExprKind::Ge, var(4), lit(2)), stmts(inc(3))))));
}

TEST(rare_values_problem_labels_and_reference_solution) {
    RareValuesProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3, 3}), 2);
    checkProblem(problem, *firstOccurrencePass(stmts(makeIf(cmp(ExprKind::Le, var(4), lit(2)), stmts(inc(3))))));
}

TEST(sum_distinct_problem_labels_and_reference_solution) {
    SumDistinctProblem problem;
    CHECK_EQ(problem.answer({4, 1, 4, 2}), 7);
    checkProblem(problem, *firstOccurrencePass(stmts(makeAssign(3, add(var(3), at(var(1)))))));
}

TEST(equal_pairs_problem_labels_and_reference_solution) {
    EqualPairsProblem problem;
    CHECK_EQ(problem.answer({1, 1, 1, 2, 2}), 4);
    checkProblem(problem, *nestedPass(true, stmts(),
                                      stmts(makeIf(cmp(ExprKind::Eq, at(var(2)), at(var(1))), stmts(inc(3)))), stmts()));
}

// -- Section 87 post-freeze problems (firstOccurrencePass from section 86) --

TEST(frequent_values_problem_labels_and_reference_solution) {
    FrequentValuesProblem problem;
    CHECK_EQ(problem.answer({1, 1, 1, 2, 2, 3, 3, 3, 3}), 2);
    checkProblem(problem, *firstOccurrencePass(stmts(makeIf(cmp(ExprKind::Gt, var(4), lit(2)), stmts(inc(3))))));
}

TEST(has_double_value_problem_labels_and_reference_solution) {
    HasDoubleValueProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3, 3}), 1);
    CHECK_EQ(problem.answer({1, 1, 1}), 0);
    checkProblem(problem, *firstOccurrencePass(stmts(makeIf(cmp(ExprKind::Eq, var(4), lit(2)), stmts(makeAssign(3, lit(1)))))));
}

TEST(odd_frequency_values_problem_labels_and_reference_solution) {
    OddFrequencyValuesProblem problem;
    CHECK_EQ(problem.answer({1, 2, 2, 3, 3, 3}), 2);
    checkProblem(problem, *firstOccurrencePass(stmts(makeIf(cmp(ExprKind::Eq, makeBinOp(ExprKind::Mod, var(4), lit(2)), lit(1)),
                                                             stmts(inc(3))))));
}
