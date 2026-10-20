#include "core/Random.hpp"
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
