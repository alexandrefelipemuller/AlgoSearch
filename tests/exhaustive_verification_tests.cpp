#include <string>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "problems/CycleDetectionProblem.hpp"
#include "problems/SortingProblem.hpp"
#include "search/Benchmark.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

ExprPtr inc(int v) { return makeBinOp(ExprKind::Add, makeVar(v), makeIntConst(1)); }

// found = exists i < j with arr[i] == arr[j], written to arr[0].
std::vector<StmtPtr> hasRepeatedValue(int i, int j, int found) {
    std::vector<StmtPtr> out;
    std::vector<StmtPtr> setFound;
    setFound.push_back(makeAssign(found, makeIntConst(1)));
    std::vector<StmtPtr> innerBody;
    innerBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeArrayAccess(makeVar(i)), makeArrayAccess(makeVar(j))),
                               std::move(setFound)));
    innerBody.push_back(makeAssign(j, inc(j)));
    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(j, inc(i)));
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(j), makeVar(0)), std::move(innerBody)));
    outerBody.push_back(makeAssign(i, inc(i)));
    out.push_back(makeAssign(found, makeIntConst(0)));
    out.push_back(makeAssign(i, makeIntConst(0)));
    out.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeVar(0)), std::move(outerBody)));
    out.push_back(makeAssignArray(makeIntConst(0), makeVar(found)));
    return out;
}

}  // namespace

TEST(exhaustive_check_rejects_section_38_cycle_false_positive) {
    // The exact shape --benchmark accepted in section 38: arr[arr[0]] = 0,
    // then "some value repeats". Passes the generator's sampled instances,
    // wrong on ~45% of all small functional graphs.
    auto program = makeBlock();
    program->body.push_back(makeAssignArray(makeArrayAccess(makeIntConst(0)), makeIntConst(0)));
    for (auto& s : hasRepeatedValue(5, 4, 7)) program->body.push_back(std::move(s));

    CycleDetectionProblem problem(6, 30, 8);
    std::string reason;
    CHECK(!verifyGeneralization(problem, *program, 1000000, reason, false));
    CHECK(reason.find("exhaustively") != std::string::npos);
}

TEST(exhaustive_check_accepts_a_correct_sort) {
    // Fung's "I can't believe it can sort", section 38's champion.
    std::vector<StmtPtr> then;
    then.push_back(makeSwap(makeVar(1), makeVar(2)));
    std::vector<StmtPtr> innerBody;
    innerBody.push_back(makeIf(makeCompare(ExprKind::Le, makeArrayAccess(makeVar(1)), makeArrayAccess(makeVar(2))),
                               std::move(then)));
    innerBody.push_back(makeAssign(2, inc(2)));
    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(2, makeIntConst(0)));
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(2), makeVar(0)), std::move(innerBody)));
    outerBody.push_back(makeAssign(1, inc(1)));
    auto program = makeBlock();
    program->body.push_back(makeAssign(1, makeIntConst(0)));
    program->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(1), makeVar(0)), std::move(outerBody)));

    SortingProblem problem(6, 40, 4);
    std::string reason;
    CHECK(verifyGeneralization(problem, *program, 10000000, reason, false));
}
