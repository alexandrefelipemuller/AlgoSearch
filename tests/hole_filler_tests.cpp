#include <vector>

#include "ast/Expr.hpp"
#include "search/HoleFiller.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// V = 3 in the branch-and-bound layout: arr[0] = nv, path at 1.., used at
// uBase = 1 + nv, weight matrix at mBase = 1 + 4 nv. State: path = [0],
// candidate c = 1, cost = w(0,1). Tours: 0->1->2->0 = 1 + 1 + 5 = 7.
struct TspState {
    std::vector<int> vars = std::vector<int>(12, 0);
    std::vector<int> arr;
    explicit TspState(int best) {
        int nv = 3, uBase = 1 + nv, mBase = 1 + 4 * nv;
        arr.assign(static_cast<size_t>(mBase + nv * nv), 0);
        arr[0] = nv;
        arr[static_cast<size_t>(uBase + 0)] = 1;  // vertex 0 is on the path
        int w[3][3] = {{0, 1, 9}, {1, 0, 1}, {5, 1, 0}};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) arr[static_cast<size_t>(mBase + i * nv + j)] = w[i][j];
        vars[1] = nv;
        vars[3] = uBase;
        vars[6] = mBase;
        vars[7] = best;
        vars[8] = 1;  // level
        vars[9] = 1;  // c
        vars[11] = 1;  // cost = w(0,1)
    }
};

}  // namespace

TEST(tsp_prune_oracle_is_unsafe_when_a_strictly_better_tour_lies_below) {
    TspState s(/*best=*/100);  // the tour of cost 7 below beats 100
    CHECK(!tspPruneIsSafe(s.vars, s.arr));
}

TEST(tsp_prune_oracle_is_safe_when_nothing_below_beats_best) {
    TspState tie(/*best=*/7);  // 7 is not strictly better than 7
    CHECK(tspPruneIsSafe(tie.vars, tie.arr));
    TspState low(/*best=*/3);
    CHECK(tspPruneIsSafe(low.vars, low.arr));
}

TEST(enumerated_conditions_include_the_hand_written_guard) {
    // cost < best must be among the candidates (vars v1..v11, cost = v11, best = v7).
    std::vector<int> vars = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    ExprPtr target = makeCompare(ExprKind::Lt, makeVar(11), makeVar(7));
    bool found = false;
    for (const auto& c : enumerateConditions(vars, 3)) found = found || exprEqual(c.get(), target.get());
    CHECK(found);
}
