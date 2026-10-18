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
