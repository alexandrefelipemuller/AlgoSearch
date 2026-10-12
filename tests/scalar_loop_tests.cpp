#include <cstdlib>
#include <string>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/AverageProblem.hpp"
#include "problems/FactorialProblem.hpp"
#include "problems/FibonacciProblem.hpp"
#include "problems/GcdProblem.hpp"
#include "problems/IsPrimeProblem.hpp"
#include "problems/PowerProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// Every statement slot draws a skeleton, and the draw is restricted to the
// scalar-loop piece (section 100).
GenConfig scalarLoopOnly(int numVars) {
    GenConfig cfg;
    cfg.numVars = numVars;
    cfg.maxLoops = 2;
    cfg.loopAccumulatorBias = 1.0;
    cfg.scalarLoop = true;
    return cfg;
}

bool solvesLevels(const Problem& problem, const Stmt& program, int fromLevel, int toLevel) {
    Interpreter interp(100000);
    for (int level = fromLevel; level <= toLevel; ++level) {
        for (const auto& inst : problem.generateInstances(level)) {
            if (problem.evaluate(program, inst, interp).status != ExecStatus::SUCCESS) return false;
        }
    }
    return true;
}

// How many of `draws` single-piece programs solve `problem` on levels 2-6.
int pieceHits(const Problem& problem, int numVars, uint64_t seed, int draws = 20000, bool indexRange = false) {
    setenv("ALGOSEARCH_FORCE_SKELETON", "scalarloop", 1);
    Random rng(seed);
    GenConfig cfg = scalarLoopOnly(numVars);
    cfg.scalarLoopIndexRange = indexRange;
    int hits = 0;
    for (int i = 0; i < draws; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        if (solvesLevels(problem, *p, 2, 6)) hits++;
    }
    unsetenv("ALGOSEARCH_FORCE_SKELETON");
    return hits;
}

}  // namespace

TEST(scalar_loop_piece_is_byte_identical_when_off) {
    GenConfig a;
    a.numVars = 5;
    GenConfig b = a;
    b.scalarLoop = false;
    Random ra(7), rb(7);
    for (int i = 0; i < 200; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(scalar_loop_piece_reaches_the_scalar_skeleton_problems) {
    CHECK(pieceHits(AverageProblem(6, 40, 5), 5, 1, 100000) > 0);
    CHECK(pieceHits(FactorialProblem(10, 40, 5), 5, 2) > 0);
    CHECK(pieceHits(PowerProblem(6, 40, 6), 6, 3) > 0);
    CHECK(pieceHits(GcdProblem(12, 40, 4), 4, 4) > 0);
    CHECK(pieceHits(IsPrimeProblem(30, 40, 5), 5, 5) > 0);
    CHECK(pieceHits(FibonacciProblem(20, 40, 5), 5, 6) > 0);
}

TEST(scalar_loop_piece_cannot_build_fibonacci_with_three_scratch_vars) {
    // Pair-Counted needs a, b, t and i; numVars 4 leaves three.
    CHECK_EQ(pieceHits(FibonacciProblem(20, 40, 4), 4, 7, 5000), 0);
}

TEST(scalar_loop_index_range_is_byte_identical_when_off) {
    GenConfig a = scalarLoopOnly(5);
    GenConfig b = a;
    b.scalarLoopIndexRange = false;
    Random ra(9), rb(9);
    for (int i = 0; i < 200; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(scalar_loop_index_range_makes_average_draws_more_frequent) {
    // Section 101 probe: about 10 -> 300 hits per 200k draws.
    int off = pieceHits(AverageProblem(6, 40, 5), 5, 12, 40000, false);
    int on = pieceHits(AverageProblem(6, 40, 5), 5, 12, 40000, true);
    CHECK(on > 5 * (off + 1));
}
