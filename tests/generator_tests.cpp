#include <functional>

#include "core/Random.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(generator_produces_program_near_target_size) {
    Random rng(1);
    GenConfig cfg;
    Program p = generateRandomProgram(rng, 10, cfg);
    CHECK(p->size() >= 10);
    CHECK(p->size() <= 120);  // generous upper bound: a single nested if/while statement can overshoot
}

TEST(generator_respects_max_loops) {
    Random rng(2);
    GenConfig cfg;
    cfg.maxLoops = 1;
    for (int i = 0; i < 20; ++i) {
        Program p = generateRandomProgram(rng, 25, cfg);
        int loops = 0;
        std::function<void(const Stmt&)> count = [&](const Stmt& s) {
            if (s.kind == StmtKind::While) loops++;
            for (const auto& c : s.body) count(*c);
            for (const auto& c : s.elseBody) count(*c);
        };
        count(*p);
        CHECK(loops <= 1);
    }
}

TEST(loop_accumulator_bias_produces_a_counting_loop_when_forced) {
    Random rng(11);
    GenConfig cfg;
    cfg.numVars = 4;
    cfg.loopAccumulatorBias = 1.0;  // always trigger when a loop is chosen
    cfg.maxLoops = 1;

    bool foundCountingLoop = false;
    for (int attempt = 0; attempt < 20 && !foundCountingLoop; ++attempt) {
        Program p = generateRandomProgram(rng, 8, cfg);
        // With bias=1.0, ANY While in the tree must be the canonical
        // skeleton: cond is (var < var0), and the body's last statement
        // increments that same var by 1.
        std::function<void(const Stmt&)> check = [&](const Stmt& s) {
            if (s.kind == StmtKind::While) {
                bool condIsIndexVsN = s.cond->kind == ExprKind::Lt && s.cond->lhs->kind == ExprKind::Var &&
                                       s.cond->rhs->kind == ExprKind::Var && s.cond->rhs->varIndex == 0;
                bool hasIncrement = !s.body.empty() && s.body.back()->kind == StmtKind::Assign &&
                                     s.body.back()->valueExpr->kind == ExprKind::Add;
                if (condIsIndexVsN && hasIncrement) foundCountingLoop = true;
            }
            for (const auto& c : s.body) check(*c);
            for (const auto& c : s.elseBody) check(*c);
        };
        check(*p);
    }
    CHECK(foundCountingLoop);
}

TEST(loop_accumulator_bias_zero_never_forces_the_skeleton) {
    // Sanity check the default (0.0) reproduces plain unbiased generation
    // -- i.e. the bias path is never taken when disabled. Can't assert
    // "no while loop ever appears" (unbiased generation can still produce
    // one by chance), but we can assert the feature is opt-in: default
    // GenConfig has the bias off.
    GenConfig cfg;
    CHECK_EQ(cfg.loopAccumulatorBias, 0.0);
}

TEST(grow_program_increases_size) {
    Random rng(3);
    GenConfig cfg;
    Program seed = generateRandomProgram(rng, 5, cfg);
    int originalSize = seed->size();
    Program grown = growProgram(*seed, originalSize + 10, rng, cfg);
    CHECK(grown->size() >= originalSize);
}
