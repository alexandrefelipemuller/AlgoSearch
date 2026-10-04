#include <string>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/MedianProblem.hpp"
#include "problems/SecondSmallestProblem.hpp"
#include "search/Benchmark.hpp"
#include "search/TailFiller.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

ExprPtr var(int v) { return makeVar(v); }
ExprPtr lit(int c) { return makeIntConst(c); }
ExprPtr cmp(ExprKind k, ExprPtr a, ExprPtr b) { return makeCompare(k, std::move(a), std::move(b)); }
StmtPtr inc(int v) { return makeAssign(v, makeBinOp(ExprKind::Add, var(v), lit(1))); }

// Selection sort of the whole array (v1, v2 indices, v4 argmin): the prefix
// section 50's near misses all shared.
Program sortPrefix() {
    std::vector<StmtPtr> inner;
    std::vector<StmtPtr> setMin;
    setMin.push_back(makeAssign(4, var(2)));
    inner.push_back(makeIf(cmp(ExprKind::Lt, makeArrayAccess(var(2)), makeArrayAccess(var(4))), std::move(setMin)));
    inner.push_back(inc(2));
    std::vector<StmtPtr> outer;
    outer.push_back(makeAssign(4, var(1)));
    outer.push_back(makeAssign(2, makeBinOp(ExprKind::Add, var(1), lit(1))));
    outer.push_back(makeWhile(cmp(ExprKind::Lt, var(2), var(0)), std::move(inner)));
    outer.push_back(makeSwap(var(1), var(4)));
    outer.push_back(inc(1));
    auto p = makeBlock();
    p->body.push_back(makeAssign(1, lit(0)));
    p->body.push_back(makeWhile(cmp(ExprKind::Lt, var(1), var(0)), std::move(outer)));
    return p;
}

std::vector<Instance> levels(const Problem& problem, int maxN) {
    std::vector<Instance> all;
    for (int n = 1; n <= maxN; ++n) {
        auto inst = problem.generateInstances(n);
        all.insert(all.end(), inst.begin(), inst.end());
    }
    return all;
}

}  // namespace

TEST(tail_filler_finds_guarded_read_after_sort_for_second_smallest) {
    SecondSmallestProblem problem;
    Interpreter interp(1000000);
    auto filled = fillTail(*sortPrefix(), levels(problem, 5), problem, interp);
    CHECK(filled != nullptr);
    std::string reason;
    CHECK(verifyGeneralization(problem, *filled, 10000000, reason, false));  // exhaustive, n = 1 included
}

TEST(tail_filler_finds_middle_read_after_sort_for_median) {
    MedianProblem problem;
    Interpreter interp(1000000);
    auto filled = fillTail(*sortPrefix(), levels(problem, 5), problem, interp);
    CHECK(filled != nullptr);
    std::string reason;
    CHECK(verifyGeneralization(problem, *filled, 10000000, reason, false));
}

TEST(tail_filler_returns_null_when_the_prefix_never_completes) {
    SecondSmallestProblem problem;
    Interpreter interp(1000);
    auto loop = makeBlock();
    std::vector<StmtPtr> body;
    body.push_back(inc(1));
    loop->body.push_back(makeWhile(cmp(ExprKind::Ge, var(0), lit(0)), std::move(body)));  // never ends
    CHECK(fillTail(*loop, levels(problem, 3), problem, interp) == nullptr);
}
