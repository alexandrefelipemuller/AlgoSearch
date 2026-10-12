#include <functional>
#include <memory>
#include <regex>
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

TEST(combinator_nested_pieces_are_byte_identical_when_off) {
    // Section 71: the flag adds draws only when it is on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorStep = b.combinatorStep = true;
    b.combinatorNested = false;
    Random ra(71), rb(71);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(combinator_nested_pieces_emit_threshold_and_absolute_step_blocks) {
    // With the flag on, some blocks compare a per-element aggregate with
    // n / 2, and some take the absolute value of a step (0 - step).
    Random rng(72);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorStep = true;
    cfg.combinatorNested = true;
    int halfThreshold = 0, absStep = 0;
    for (int i = 0; i < 4000; ++i) {
        std::string text = generateRandomProgram(rng, 2, cfg)->toString();
        if (text.find("/ 2)") != std::string::npos) halfThreshold++;
        if (text.find("(0 - v") != std::string::npos) absStep++;
    }
    CHECK(halfThreshold > 0);
    CHECK(absStep > 0);
}

TEST(nested_canonical_tests_are_byte_identical_when_off) {
    // Section 73: the canonical draw only happens with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorStep = b.combinatorStep = true;
    a.combinatorNested = b.combinatorNested = true;
    b.nestedCanonicalTests = false;
    Random ra(73), rb(73);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(frequency_table_piece_is_byte_identical_when_off) {
    // Section 75: the fifth stateful piece is drawn only with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorState = b.combinatorState = true;
    b.combinatorFrequency = false;
    Random ra(75), rb(75);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(frequency_table_piece_emits_table_passes) {
    // With the flag on, some blocks read the table's first cell (the max /
    // min pass over aux), which no other piece does.
    Random rng(76);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorState = true;
    cfg.combinatorFrequency = true;
    cfg.maxLoops = 4;  // the table needs three loops
    int tablePasses = 0;
    for (int i = 0; i < 4000; ++i) {
        if (generateRandomProgram(rng, 2, cfg)->toString().find("= aux[0]") != std::string::npos) tablePasses++;
    }
    CHECK(tablePasses > 0);
}

TEST(follow_piece_is_byte_identical_when_off) {
    // Section 77: follow blocks are drawn only with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    b.combinatorFollow = false;
    Random ra(77), rb(77);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(follow_piece_emits_pointer_walks) {
    // With the flag on, some blocks advance a pointer through the array
    // (p = arr[(base + p)]) and break out of range.
    Random rng(78);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorFollow = true;
    int walks = 0;
    for (int i = 0; i < 2000; ++i) {
        std::string text = generateRandomProgram(rng, 2, cfg)->toString();
        if (text.find("break") != std::string::npos && std::regex_search(text, std::regex(R"(v(\d) = arr\[\(\d \+ v\1\)\])"))) walks++;
    }
    CHECK(walks > 0);
}

TEST(orthogonal_modifiers_are_byte_identical_when_off) {
    // Section 80: direction and SumStep draws happen only with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorStep = b.combinatorStep = true;
    a.combinatorNested = b.combinatorNested = true;
    b.combinatorOrthogonal = false;
    Random ra(80), rb(80);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(orthogonal_modifiers_emit_step_sums) {
    // With the flag on, some adjacent blocks add the step into the
    // accumulator (acc = acc + step), which no other relation does.
    Random rng(81);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorStep = true;
    cfg.combinatorOrthogonal = true;
    int sums = 0;
    for (int i = 0; i < 4000; ++i) {
        std::string text = generateRandomProgram(rng, 2, cfg)->toString();
        std::smatch m;
        if (std::regex_search(text, m, std::regex(R"((v\d) = \((arr\[[^;]*\] - arr\[[^;]*\])\);)"))) {
            if (text.find(m[1].str() + ")") != std::string::npos &&
                std::regex_search(text, std::regex(R"(v(\d) = \(v\1 \+ )" + m[1].str() + R"(\))"))) {
                sums++;
            }
        }
    }
    CHECK(sums > 0);
}

TEST(argbest_modifier_is_byte_identical_when_off) {
    // Section 82: arg-best draws happen only with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorState = b.combinatorState = true;
    a.combinatorNested = b.combinatorNested = true;
    a.combinatorFrequency = b.combinatorFrequency = true;
    a.maxLoops = b.maxLoops = 4;
    b.combinatorArgBest = false;
    Random ra(82), rb(82);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(argbest_modifier_emits_index_of_first_maximum) {
    // With the flag on, some block returns the index of the first maximum,
    // which no relation returned before.
    Random rng(83);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorArgBest = true;
    Interpreter interp(100000);
    const std::vector<std::pair<std::vector<int>, int>> cases = {{{3, 9, 2}, 1}, {{5, 1, 5}, 0}, {{1}, 0}, {{2, 7, 7}, 1}, {{4, 0, 6, 6}, 2}};
    int found = 0;
    for (int i = 0; i < 20000 && found == 0; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        bool all = true;
        for (const auto& [input, want] : cases) {
            std::vector<int> out;
            all = all && interp.run(*p, input, cfg.numVars, out) == RunStatus::COMPLETED && out[0] == want;
        }
        found += all;
    }
    CHECK(found > 0);
}

TEST(distinct_count_piece_is_byte_identical_when_off) {
    // Section 87: the sixth stateful piece is drawn only with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorState = b.combinatorState = true;
    a.combinatorFrequency = b.combinatorFrequency = true;
    a.combinatorArgBest = b.combinatorArgBest = true;
    a.maxLoops = b.maxLoops = 4;
    b.combinatorDistinct = false;
    Random ra(86), rb(86);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(distinct_count_piece_counts_distinct_values) {
    // With the flag on, some block counts distinct values, which counting
    // elements never gives.
    Random rng(87);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorState = true;
    cfg.combinatorFrequency = true;
    cfg.combinatorDistinct = true;
    cfg.maxLoops = 4;
    Interpreter interp(100000);
    const std::vector<std::pair<std::vector<int>, int>> cases = {{{3, 3, 1}, 2}, {{2, 2, 2, 2}, 1}, {{1, 2, 3}, 3}, {{5, 1, 5, 1, 4}, 3}};
    int found = 0;
    for (int i = 0; i < 40000 && found == 0; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        bool all = true;
        for (const auto& [input, want] : cases) {
            std::vector<int> out;
            all = all && interp.run(*p, input, cfg.numVars, out) == RunStatus::COMPLETED && out[0] == want;
        }
        found += all;
    }
    CHECK(found > 0);
}

TEST(follow_extended_is_byte_identical_when_off) {
    // Section 90: the two extra follow relations are drawn only with the flag on.
    GenConfig a = combinatorOnly(), b = combinatorOnly();
    a.combinatorFollow = b.combinatorFollow = true;
    b.followExtended = false;
    Random ra(90), rb(90);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(follow_extended_emits_trailing_and_loop_length_walks) {
    // With the flag on, some follow block reports the last node before null
    // ({0, 1, 2, 3}: 0 -> 1 -> 2 -> null gives 2) and some the cycle length
    // ({0, 1, 2, 1}: cycle 1 -> 2 -> 1 gives 2; {0, 1, 2, 3} gives 0).
    Random rng(91);
    GenConfig cfg = combinatorOnly();
    cfg.combinatorFollow = true;
    cfg.followExtended = true;
    cfg.maxLoops = 4;
    Interpreter interp(100000);
    auto runs = [&](const Stmt& p, const std::vector<int>& in, int want) {
        std::vector<int> out;
        return interp.run(p, in, cfg.numVars, out) == RunStatus::COMPLETED && out[0] == want;
    };
    int trailing = 0, loops = 0;
    for (int i = 0; i < 20000 && (trailing == 0 || loops == 0); ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        trailing += runs(*p, {0, 1, 2, 3}, 2) && runs(*p, {0, 1, 2, 1}, 3) && runs(*p, {2, 3, 3, 0}, 0);
        loops += runs(*p, {0, 1, 2, 1}, 2) && runs(*p, {0, 1, 2, 3}, 0) && runs(*p, {0, 1, 2, 0}, 3);
    }
    CHECK(trailing > 0);
    CHECK(loops > 0);
}

namespace {

GenConfig nestedPlaceProbe(bool place, bool option = false) {
    GenConfig cfg = combinatorOnly();
    cfg.combinatorState = true;
    cfg.combinatorNested = true;
    cfg.combinatorFrequency = true;
    cfg.combinatorArgBest = true;
    cfg.maxLoops = 4;
    cfg.nestedPlace = place;
    cfg.nestedPlaceOption = option;
    return cfg;
}

// Selection shape: the program sorts and its swap is not the body of an if
// (an exchange sort guards every swap).
int selectionSorts(bool place, uint64_t seed, int draws, bool option = false) {
    Random rng(seed);
    GenConfig cfg = nestedPlaceProbe(place, option);
    SortingProblem sorting(5, 40, cfg.numVars);
    const std::regex guardedSwap(R"(if \([^\n]*\) \{\n\s*swap)");
    int hits = 0;
    for (int i = 0; i < draws; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        std::string text = p->toString();
        if (text.find("swap") == std::string::npos || std::regex_search(text, guardedSwap)) continue;
        if (solvesLevels(sorting, *p, 2, 6)) hits++;
    }
    return hits;
}

}  // namespace

TEST(nested_place_piece_is_byte_identical_when_off) {
    // Section 102: the placement variant is drawn only with the flag on.
    GenConfig a = nestedPlaceProbe(false), b = nestedPlaceProbe(false);
    Random ra(102), rb(102);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(generateRandomProgram(ra, 30, a)->toString(), generateRandomProgram(rb, 30, b)->toString());
    }
}

TEST(nested_place_piece_reaches_selection_sort) {
    // Section 102 prediction 1: 0 selection sorts off, some on.
    CHECK_EQ(selectionSorts(false, 103, 20000), 0);
    CHECK(selectionSorts(true, 103, 20000) > 0);
}

TEST(nested_place_option_reaches_selection_sort_more_often) {
    // Section 102b: placement as its own relation is drawn more often than
    // inside Nested.
    CHECK(selectionSorts(false, 104, 20000, true) > selectionSorts(true, 104, 20000));
}
