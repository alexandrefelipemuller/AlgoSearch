#include "problems/Psb2Problem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(psb2_references_match_the_official_examples) {
    using K = Psb2Problem::Kind;
    CHECK_EQ(Psb2Problem::reference(K::FuelCost, {6, 9, 12})[0], 0 + 1 + 2);
    CHECK_EQ(Psb2Problem::reference(K::Basement, {5, -3, -4, 1})[0], 2);
    CHECK_EQ(Psb2Problem::reference(K::Basement, {1, 2})[0], -1);
    CHECK_EQ(Psb2Problem::reference(K::Gcd, {4200, 3528})[0], 168);
    auto pair = Psb2Problem::reference(K::FindPair, {2500, 7, 6352, 8852});
    CHECK_EQ(pair[0], 2500);
    CHECK_EQ(pair[1], 6352);
    auto coins = Psb2Problem::reference(K::CoinSums, {41, 0, 0, 0});
    CHECK_EQ(coins[0], 1);
    CHECK_EQ(coins[1], 1);
    CHECK_EQ(coins[2], 1);
    CHECK_EQ(coins[3], 1);
    CHECK_EQ(Psb2Problem::reference(K::Luhn, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5, 4, 3})[0], 80);
    CHECK_EQ(Psb2Problem::reference(K::PairedDigits, {9, 9})[0], 9);
    CHECK_EQ(Psb2Problem::reference(K::PairedDigits, {8, 3})[0], 0);
}

TEST(psb2_problem_samples_train_and_test_from_official_lines_and_checks_the_reference) {
    std::vector<std::string> edge = {R"({"input1":[6],"output1":0})", R"({"input1":[9],"output1":1})"};
    std::vector<std::string> random;
    for (int v = 6; v < 60; ++v) random.push_back("{\"input1\":[" + std::to_string(v) + "],\"output1\":" + std::to_string(v / 3 - 2) + "}");
    Psb2Problem p(Psb2Problem::Kind::FuelCost, 7, 3, edge, random, /*trainSize=*/10, /*testSize=*/20);
    CHECK_EQ(p.mismatchesAgainstOfficial(), 0u);
    CHECK_EQ(p.generateInstances(1).size(), 10u);
    CHECK_EQ(p.generateInstances(5).size(), 10u);  // the same training set at every level
    CHECK_EQ(p.generateAdversarialInstances(1).size(), 20u);
    for (const auto& inst : p.generateInstances(1)) CHECK(p.isSolution(inst.input, inst.expectedOutput));
    CHECK(!p.isSolution({12}, {0}));
}

TEST(psb2_problem_reports_a_disagreeing_official_case) {
    std::vector<std::string> edge = {R"({"input1":[9],"output1":7})"};
    Psb2Problem p(Psb2Problem::Kind::FuelCost, 7, 1, edge, edge, 1, 1);
    CHECK(p.mismatchesAgainstOfficial() > 0);
}

TEST(psb2_fuel_cost_known_loop_solution_passes_the_official_cases) {
    using namespace algosearch;
    // v6 = 0; v4 = 0; while (v4 < v0) { v6 = v6 + (arr[v4] / 3 - 2); v4 = v4 + 1; } arr[0] = v6;
    auto p = makeBlock();
    p->body.push_back(makeAssign(6, makeIntConst(0)));
    p->body.push_back(makeAssign(4, makeIntConst(0)));
    std::vector<StmtPtr> body;
    body.push_back(makeAssign(6, makeBinOp(ExprKind::Add, makeVar(6),
        makeBinOp(ExprKind::Sub, makeBinOp(ExprKind::Div, makeArrayAccess(makeVar(4)), makeIntConst(3)), makeIntConst(2)))));
    body.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(4), makeIntConst(1))));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(4), makeVar(0)), std::move(body)));
    p->body.push_back(makeAssignArray(makeIntConst(0), makeVar(6)));
    Psb2Problem prob(Psb2Problem::Kind::FuelCost, 7, 1);
    Interpreter interp(20000);
    int bad = 0, total = 0;
    for (const auto& inst : prob.generateAdversarialInstances(1)) {
        ++total;
        auto r = prob.evaluate(*p, inst, interp);
        if (r.status != ExecStatus::SUCCESS) ++bad;
    }
    (void)total;
    CHECK_EQ(bad, 0);
}

namespace {
int psb2FailuresOn(algosearch::Psb2Problem& prob, const algosearch::Stmt& program) {
    using namespace algosearch;
    Interpreter interp(20000);
    int bad = 0;
    for (const auto& inst : prob.generateAdversarialInstances(1)) {
        if (prob.evaluate(program, inst, interp).status != ExecStatus::SUCCESS) ++bad;
    }
    return bad;
}
}  // namespace

TEST(psb2_basement_known_loop_solution_passes_the_official_cases) {
    using namespace algosearch;
    // v6 = 0; v4 = 0; v5 = 0 - 1;
    // while (v4 < v0) { v6 = v6 + arr[v4]; if (v6 < 0) { v5 = v4; break; } v4 = v4 + 1; } arr[0] = v5;
    auto p = makeBlock();
    p->body.push_back(makeAssign(6, makeIntConst(0)));
    p->body.push_back(makeAssign(4, makeIntConst(0)));
    p->body.push_back(makeAssign(5, makeBinOp(ExprKind::Sub, makeIntConst(0), makeIntConst(1))));
    std::vector<StmtPtr> hit;
    hit.push_back(makeAssign(5, makeVar(4)));
    hit.push_back(makeBreak());
    std::vector<StmtPtr> body;
    body.push_back(makeAssign(6, makeBinOp(ExprKind::Add, makeVar(6), makeArrayAccess(makeVar(4)))));
    body.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(6), makeIntConst(0)), std::move(hit)));
    body.push_back(makeAssign(4, makeBinOp(ExprKind::Add, makeVar(4), makeIntConst(1))));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(4), makeVar(0)), std::move(body)));
    p->body.push_back(makeAssignArray(makeIntConst(0), makeVar(5)));
    Psb2Problem prob(Psb2Problem::Kind::Basement, 7, 1);
    CHECK_EQ(psb2FailuresOn(prob, *p), 0);
}

TEST(psb2_find_pair_known_nested_loop_solution_passes_the_official_cases) {
    using namespace algosearch;
    // v3 = arr[v0-1]; v1 = 0; v5 = 0; v6 = 0;
    // while (v1 < v0-2) { v2 = v1+1; while (v2 < v0-1) {
    //   if (arr[v1] + arr[v2] == v3) { v5 = arr[v1]; v6 = arr[v2]; v1 = v0; break; } v2 = v2+1; } v1 = v1+1; }
    // arr[0] = v5; arr[1] = v6;
    auto p = makeBlock();
    auto lenMinus = [](int k) { return makeBinOp(ExprKind::Sub, makeVar(0), makeIntConst(k)); };
    p->body.push_back(makeAssign(3, makeArrayAccess(lenMinus(1))));
    p->body.push_back(makeAssign(1, makeIntConst(0)));
    p->body.push_back(makeAssign(5, makeIntConst(0)));
    p->body.push_back(makeAssign(6, makeIntConst(0)));
    std::vector<StmtPtr> hit;
    hit.push_back(makeAssign(5, makeArrayAccess(makeVar(1))));
    hit.push_back(makeAssign(6, makeArrayAccess(makeVar(2))));
    hit.push_back(makeAssign(1, makeVar(0)));
    hit.push_back(makeBreak());
    std::vector<StmtPtr> inner;
    inner.push_back(makeIf(makeCompare(ExprKind::Eq,
        makeBinOp(ExprKind::Add, makeArrayAccess(makeVar(1)), makeArrayAccess(makeVar(2))), makeVar(3)),
        std::move(hit)));
    inner.push_back(makeAssign(2, makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1))));
    std::vector<StmtPtr> outer;
    outer.push_back(makeAssign(2, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1))));
    outer.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(2), lenMinus(1)), std::move(inner)));
    outer.push_back(makeAssign(1, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(1))));
    p->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(1), lenMinus(2)), std::move(outer)));
    p->body.push_back(makeAssignArray(makeIntConst(0), makeVar(5)));
    p->body.push_back(makeAssignArray(makeIntConst(1), makeVar(6)));
    Psb2Problem prob(Psb2Problem::Kind::FindPair, 8, 1);
    CHECK_EQ(psb2FailuresOn(prob, *p), 0);
}
