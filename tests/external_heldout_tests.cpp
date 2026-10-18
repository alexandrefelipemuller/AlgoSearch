#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "search/Benchmark.hpp"

#include "problems/EquilibriumPointProblem.hpp"
#include "problems/AnyDuplicateProblem.hpp"
#include "problems/AnyPeakProblem.hpp"
#include "problems/ContainsAscendingProblem.hpp"
#include "problems/AnyValleyProblem.hpp"
#include "problems/PeakElementProblem.hpp"
#include "problems/SearchRotatedProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(section_114_problems_compute_their_answers) {
    CHECK_EQ(PeakElementProblem().answer({1, 3, 2, 5}), 1);
    CHECK_EQ(PeakElementProblem().answer({4, 1}), 0);
    CHECK_EQ(PeakElementProblem().answer({}), 0);
    CHECK_EQ(SearchRotatedProblem().answer({5, 7, 9, 1, 3, 5}), 5);
    CHECK_EQ(SearchRotatedProblem().answer({4, 7, 9, 1, 3}), -1);
    CHECK_EQ(EquilibriumPointProblem().answer({1, 3, 5, 2, 2}), 2);
    CHECK_EQ(EquilibriumPointProblem().answer({1, 2}), -1);
    CHECK_EQ(EquilibriumPointProblem().answer({}), -1);
}

TEST(section_114_instances_are_labelled_by_answer) {
    PeakElementProblem peak;
    SearchRotatedProblem search;
    EquilibriumPointProblem balance;
    for (int level = 1; level <= 6; ++level) {
        for (const ScalarFromArrayProblem* p : std::vector<const ScalarFromArrayProblem*>{&peak, &search, &balance}) {
            auto instances = p->generateInstances(level);
            CHECK(!instances.empty());
            for (const auto& inst : instances) {
                std::vector<int> out = inst.input;
                if (!out.empty()) out[0] = inst.expectedOutput[0];
                else out.push_back(inst.expectedOutput[0]);
                CHECK(p->isSolution(inst.input, out));
            }
        }
    }
}

TEST(section_114_constructed_instances_hit_the_rare_labels) {
    // search_rotated finds the key and equilibrium_point balances in some instances.
    int found = 0, balanced = 0;
    for (const auto& inst : SearchRotatedProblem().generateInstances(6)) found += inst.expectedOutput[0] > 0;
    for (const auto& inst : EquilibriumPointProblem().generateInstances(6)) balanced += inst.expectedOutput[0] >= 0;
    CHECK(found > 0);
    CHECK(balanced > 0);
}

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

bool solvesEverything(const ScalarFromArrayProblem& problem, const Stmt& reference) {
    Interpreter interp(1000000);
    for (int n = 1; n <= 8; ++n) {
        for (const auto& inst : problem.generateInstances(n)) {
            if (problem.evaluate(reference, inst, interp).status != ExecStatus::SUCCESS) return false;
        }
    }
    std::string reason;
    return verifyGeneralization(problem, reference, 10000000, reason, false);
}

}  // namespace

TEST(equilibrium_point_has_a_two_pass_reference_solution) {
    // v4 = total; then v5 = prefix, and the first i with prefix == total - prefix - a[i]
    // goes to v3 (-1 until found): two loops sharing v4, a -1 default, a first-match guard.
    auto p = makeBlock();
    p->body.push_back(makeAssign(4, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), stmts(makeAssign(4, add(var(4), at(var(1)))), inc(1))));
    p->body.push_back(makeAssign(3, sub(lit(0), lit(1))));
    p->body.push_back(makeAssign(5, lit(0)));
    p->body.push_back(makeAssign(1, lit(0)));
    auto hit = cmp(ExprKind::Eq, var(5), sub(sub(var(4), var(5)), at(var(1))));
    auto body = stmts(makeIf(cmp(ExprKind::Lt, var(3), lit(0)), stmts(makeIf(std::move(hit), stmts(makeAssign(3, var(1)))))),
                      makeAssign(5, add(var(5), at(var(1)))), inc(1));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(body)));
    p->body.push_back(makeAssignArray(lit(0), var(3)));
    CHECK(solvesEverything(EquilibriumPointProblem(), *p));
}

TEST(section_120_open_specification_checkers_accept_every_valid_answer) {
    AnyPeakProblem peak;
    CHECK(peak.isSolution({1, 3, 2, 5}, {1, 0, 0, 0}));
    CHECK(peak.isSolution({1, 3, 2, 5}, {3, 0, 0, 0}));
    CHECK(!peak.isSolution({1, 3, 2, 5}, {0, 0, 0, 0}));
    CHECK(!peak.isSolution({1, 3, 2, 5}, {4, 0, 0, 0}));
    AnyDuplicateProblem dup;
    CHECK(dup.isSolution({2, 5, 2, 5}, {2, 0, 0, 0}));
    CHECK(dup.isSolution({2, 5, 2, 5}, {5, 0, 0, 0}));
    CHECK(!dup.isSolution({2, 5, 2, 7}, {5, 0, 0, 0}));
    CHECK(dup.isSolution({1, 2, 3}, {-1, 0, 0}));
    CHECK(!dup.isSolution({1, 1, 3}, {-1, 0, 0}));
}

TEST(section_120_instance_labels_pass_their_own_checkers) {
    AnyPeakProblem peak;
    AnyDuplicateProblem dup;
    for (int level = 1; level <= 6; ++level) {
        for (const ScalarFromArrayProblem* p : std::vector<const ScalarFromArrayProblem*>{&peak, &dup}) {
            for (const auto& inst : p->generateInstances(level)) {
                std::vector<int> out = inst.input;
                out[0] = inst.expectedOutput[0];
                CHECK(p->isSolution(inst.input, out));
            }
        }
    }
}

TEST(section_121_any_valley_accepts_every_local_minimum) {
    AnyValleyProblem valley;
    CHECK(valley.isSolution({3, 1, 2, 0}, {1, 0, 0, 0}));
    CHECK(valley.isSolution({3, 1, 2, 0}, {3, 0, 0, 0}));
    CHECK(!valley.isSolution({3, 1, 2, 0}, {0, 0, 0, 0}));
    CHECK(!valley.isSolution({3, 1, 2, 0}, {2, 0, 0, 0}));
    for (const auto& inst : valley.generateInstances(5)) {
        std::vector<int> out = inst.input;
        out[0] = inst.expectedOutput[0];
        CHECK(valley.isSolution(inst.input, out));
    }
}

TEST(section_121c_contains_ascending_has_only_ascending_haystacks) {
    ContainsAscendingProblem p;
    for (int level = 2; level <= 5; ++level) {
        auto instances = p.generateInstances(level);
        CHECK(!instances.empty());
        for (const auto& inst : instances) CHECK(std::is_sorted(inst.input.begin() + 1, inst.input.end()));
    }
}

TEST(section_122_contains_ordered_adversarial_covers_one_element_haystacks) {
    ContainsOrderedProblem p;
    bool singleAbsent = false, singlePresent = false;
    for (const auto& inst : p.generateAdversarialInstances(4)) {
        if (inst.input.size() == 2) (inst.expectedOutput[0] ? singlePresent : singleAbsent) = true;
    }
    CHECK(singleAbsent);
    CHECK(singlePresent);
}
