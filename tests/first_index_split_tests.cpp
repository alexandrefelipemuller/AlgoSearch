#include <string>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/CountBalancePointsProblem.hpp"
#include "problems/EquilibriumPointProblem.hpp"
#include "problems/FirstGreaterThanFirstProblem.hpp"
#include "problems/FirstSplitTwoToOneProblem.hpp"
#include "problems/SearchRotatedProblem.hpp"
#include "problems/SecondIndexOfKeyProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// The current default's combinator family, with the section 117 flags.
GenConfig splitProbe(bool firstIndex, bool prefixSplit) {
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.maxLoops = 4;
    cfg.loopAccumulatorBias = 0.0;
    cfg.combinatorBias = 1.0;
    cfg.combinatorStep = true;
    cfg.combinatorState = true;
    cfg.combinatorFrequency = true;
    cfg.combinatorArgBest = true;
    cfg.combinatorDistinct = true;
    cfg.distinctAggregate = true;
    cfg.argSentinel = true;
    cfg.firstIndex = firstIndex;
    cfg.prefixSplit = prefixSplit;
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

int hits(const Problem& problem, bool firstIndex, bool prefixSplit, uint64_t seed, int draws = 20000) {
    Random rng(seed);
    GenConfig cfg = splitProbe(firstIndex, prefixSplit);
    int count = 0;
    for (int i = 0; i < draws; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        if (solvesLevels(problem, *p, 2, 6)) count++;
    }
    return count;
}

}  // namespace

TEST(section_117_problems_compute_their_answers) {
    CHECK_EQ(FirstGreaterThanFirstProblem().answer({3, 1, 3, 5, 9}), 3);
    CHECK_EQ(FirstGreaterThanFirstProblem().answer({3, 1}), -1);
    CHECK_EQ(SecondIndexOfKeyProblem().answer({2, 2, 0, 2, 2}), 3);
    CHECK_EQ(SecondIndexOfKeyProblem().answer({2, 2}), -1);
    CHECK_EQ(CountBalancePointsProblem().answer({1, 0, 0, 1}), 2);
    CHECK_EQ(FirstSplitTwoToOneProblem().answer({4, 9, 2}), 1);
    CHECK_EQ(FirstSplitTwoToOneProblem().answer({1, 1}), -1);
}

TEST(first_index_and_prefix_split_are_byte_identical_when_off) {
    GenConfig a = splitProbe(false, false), b = splitProbe(false, false);
    GenConfig c = splitProbe(true, true);
    Random ra(117), rb(117), rc(117);
    int differ = 0;
    for (int i = 0; i < 300; ++i) {
        std::string ta = generateRandomProgram(ra, 30, a)->toString();
        CHECK_EQ(ta, generateRandomProgram(rb, 30, b)->toString());
        if (ta != generateRandomProgram(rc, 30, c)->toString()) differ++;
    }
    CHECK(differ > 0);
}

TEST(first_index_and_prefix_split_reach_their_targets_and_not_their_rulers) {
    // Section 117 predictions, at the piece level.
    CHECK_EQ(hits(SearchRotatedProblem(), false, false, 1), 0);
    CHECK(hits(SearchRotatedProblem(), true, false, 1) > 0);
    CHECK(hits(FirstGreaterThanFirstProblem(), true, false, 2) > 0);
    CHECK_EQ(hits(EquilibriumPointProblem(), true, false, 3), 0);
    CHECK(hits(EquilibriumPointProblem(), true, true, 3) > 0);
    CHECK(hits(CountBalancePointsProblem(), true, true, 4) > 0);
    CHECK_EQ(hits(SecondIndexOfKeyProblem(), true, true, 5), 0);
    CHECK_EQ(hits(FirstSplitTwoToOneProblem(), true, true, 6), 0);
}
