#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/ContainsProblem.hpp"
#include "problems/CountDescentsProblem.hpp"
#include "problems/HasAdjacentEqualProblem.hpp"
#include "problems/IsSortedProblem.hpp"
#include "problems/StrictlyIncreasingProblem.hpp"
#include "problems/CountProblem.hpp"
#include "problems/MaxArrayProblem.hpp"
#include "problems/MinArrayProblem.hpp"
#include "problems/ReverseProblem.hpp"
#include "problems/SingleNumberProblem.hpp"
#include "problems/SortingProblem.hpp"
#include "problems/SubstringProblem.hpp"
#include "problems/SumArrayProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

GenConfig combinatorOnly() {
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.maxLoops = 2;
    cfg.loopAccumulatorBias = 0.0;
    cfg.combinatorBias = 1.0;
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

// Phase-1 style reachability (same protocol as sections 18/31): does the
// combinator family CONTAIN a program that solves `problem` on levels 2-6?
// If not, no amount of search could find one through this operator.
bool familyReaches(const Problem& problem, uint64_t seed, bool adjacent = true) {
    Random rng(seed);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorAdjacent = adjacent;
    for (int i = 0; i < 4000; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);  // an empty root is already size 1
        if (solvesLevels(problem, *p, 2, 6)) return true;
    }
    return false;
}

}  // namespace

TEST(combinator_family_reaches_catalogued_array_problems) {
    // numVars = 8 everywhere: the block's binding context uses v0..v7.
    CHECK(familyReaches(SumArrayProblem(6, 40, 8), 1));
    CHECK(familyReaches(MinArrayProblem(6, 40, 8), 2));
    CHECK(familyReaches(MaxArrayProblem(6, 40, 8), 3));
    CHECK(familyReaches(ContainsProblem(6, 40, 8), 4));
    CHECK(familyReaches(CountProblem(6, 40, 8), 5));
    CHECK(familyReaches(SingleNumberProblem(4, 40, 8), 6));
    CHECK(familyReaches(ReverseProblem(6, 40, 8), 7));
    CHECK(familyReaches(SortingProblem(6, 40, 8), 8));
    CHECK(familyReaches(SubstringProblem(5, 30, 8), 9));
}

TEST(combinator_block_is_a_single_top_level_statement) {
    // With combinatorBias on, a fresh program stops growing right after the
    // self-contained block, so no sibling can clobber its answer.
    Random rng(10);
    GenConfig cfg = combinatorOnly();
    int blocks = 0;
    for (int i = 0; i < 200; ++i) {
        Program p = generateRandomProgram(rng, 30, cfg);
        if (!p->body.empty() && p->body.back()->kind == StmtKind::Block) blocks++;
    }
    CHECK(blocks > 150);
}

TEST(combinator_default_family_reaches_adjacent_pair_problems) {
    // Section 42: the adjacent-pair loop is part of the default product...
    CHECK(familyReaches(IsSortedProblem(8), 11));
    CHECK(familyReaches(CountDescentsProblem(8), 12));
    CHECK(familyReaches(StrictlyIncreasingProblem(8), 13));
    CHECK(familyReaches(HasAdjacentEqualProblem(8), 14));
}

TEST(combinator_v1_family_does_not_reach_is_sorted) {
    // ...and --combinator-v1 still reproduces the frozen family without it.
    CHECK(!familyReaches(IsSortedProblem(8), 11, /*adjacent=*/false));
}

TEST(combinator_step_axes_are_byte_identical_when_off) {
    // Section 52: the flag only adds draws on an adjacent pass when it is on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    b.combinatorStep = false;
    Random ra(77), rb(77);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(combinator_step_axes_emit_step_and_segment_blocks) {
    // With the flag on, some blocks compute a step (elem(i) - elem(i-1)) and
    // some count segments (an accumulator initialized to 1 on an adjacent pass).
    Random rng(5);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorStep = true;
    int steps = 0;
    for (int i = 0; i < 2000; ++i) {
        std::string text = generateRandomProgram(rng, 2, cfg)->toString();
        if (text.find("] - arr[") != std::string::npos) steps++;
    }
    CHECK(steps > 0);
}

TEST(combinator_state_pieces_are_byte_identical_when_off) {
    // Section 55: the flag adds a draw only when it is on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    b.combinatorState = false;
    Random ra(91), rb(91);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(combinator_state_pieces_emit_aux_dp_blocks) {
    // With the flag on, some blocks write aux (EarlierCellsBest / PrefixSuffix).
    Random rng(6);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorState = true;
    int auxWrites = 0;
    for (int i = 0; i < 2000; ++i) {
        if (generateRandomProgram(rng, 2, cfg)->toString().find("aux[") != std::string::npos) auxWrites++;
    }
    CHECK(auxWrites > 0);
}

TEST(adaptive_family_weights_are_byte_identical_when_off) {
    // Section 59: familyWeight is never read unless adaptiveFamilies is on.
    GenConfig a = combinatorOnly();
    a.loopAccumulatorBias = 0.4;
    a.combinatorBias = 0.5;
    a.combinatorState = true;
    GenConfig b = a;
    for (int f = 0; f < kFamilyCount; ++f) b.familyWeight[static_cast<size_t>(f)] = f % 2 ? 4.0 : 0.5;
    Random ra(59), rb(59);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(family_draws_tag_combinator_blocks_and_reset) {
    Random rng(60);
    GenConfig cfg = combinatorOnly();
    takeFamilyDraws();
    generateRandomProgram(rng, 2, cfg);
    CHECK((takeFamilyDraws() & (1u << FamilyCombinatorPlain)) != 0);
    CHECK_EQ(takeFamilyDraws(), 0u);
}

TEST(adaptive_family_weights_shift_the_stateful_share) {
    // Weight 4 vs 0.5 on the stateful family moves its share of combinator
    // blocks well past the static quarter, and the reverse well below it.
    auto statefulDraws = [](double state, double plain) {
        Random rng(61);
        GenConfig cfg = combinatorOnly();
        cfg.combinatorState = true;
        cfg.adaptiveFamilies = true;
        cfg.familyWeight[FamilyCombinatorState] = state;
        cfg.familyWeight[FamilyCombinatorPlain] = plain;
        int count = 0;
        takeFamilyDraws();
        for (int i = 0; i < 1000; ++i) {
            generateRandomProgram(rng, 2, cfg);
            if (takeFamilyDraws() & (1u << FamilyCombinatorState)) count++;
        }
        return count;
    };
    CHECK(statefulDraws(4.0, 0.5) > 600);
    CHECK(statefulDraws(0.5, 4.0) < 100);
}
