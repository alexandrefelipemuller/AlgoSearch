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

TEST(grow_program_increases_size) {
    Random rng(3);
    GenConfig cfg;
    Program seed = generateRandomProgram(rng, 5, cfg);
    int originalSize = seed->size();
    Program grown = growProgram(*seed, originalSize + 10, rng, cfg);
    CHECK(grown->size() >= originalSize);
}
