#include <string>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/FrequentModeOrNoneProblem.hpp"
#include "problems/LongestIncreasingRunEndProblem.hpp"
#include "problems/LongestRunStartProblem.hpp"
#include "problems/LongestRunValueProblem.hpp"
#include "problems/MajorityValueProblem.hpp"
#include "problems/PluralityValueProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// The current default's stateful family (section 113 adds two axes to it).
GenConfig argSentinelProbe(bool on) {
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.maxLoops = 4;
    cfg.loopAccumulatorBias = 0.0;
    cfg.combinatorBias = 1.0;
    cfg.combinatorState = true;
    cfg.combinatorFrequency = true;
    cfg.combinatorArgBest = true;
    cfg.combinatorDistinct = true;
    cfg.distinctAggregate = true;
    cfg.argSentinel = on;
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

int argSentinelHits(const Problem& problem, bool on, uint64_t seed, int draws = 40000) {
    Random rng(seed);
    GenConfig cfg = argSentinelProbe(on);
    int hits = 0;
    for (int i = 0; i < draws; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        if (solvesLevels(problem, *p, 2, 6)) hits++;
    }
    return hits;
}

}  // namespace

TEST(section_113_problems_compute_their_answers) {
    CHECK_EQ(LongestIncreasingRunEndProblem().answer({5, 1, 2, 3, 0, 4}), 3);
    CHECK_EQ(LongestIncreasingRunEndProblem().answer({}), 0);
    CHECK_EQ(FrequentModeOrNoneProblem().answer({3, 1, 1, 3}), 3);
    CHECK_EQ(FrequentModeOrNoneProblem().answer({3, 1, 2}), -1);
    CHECK_EQ(LongestRunStartProblem().answer({1, 2, 2, 0, 0}), 1);
    CHECK_EQ(PluralityValueProblem().answer({2, 1, 2}), 2);
    CHECK_EQ(PluralityValueProblem().answer({2, 1, 2, 1}), -1);
}

TEST(arg_sentinel_is_byte_identical_when_off) {
    GenConfig a = argSentinelProbe(false), b = argSentinelProbe(false), c = argSentinelProbe(true);
    Random ra(113), rb(113), rc(113);
    int differ = 0;
    for (int i = 0; i < 300; ++i) {
        std::string ta = generateRandomProgram(ra, 30, a)->toString();
        CHECK_EQ(ta, generateRandomProgram(rb, 30, b)->toString());
        if (ta != generateRandomProgram(rc, 30, c)->toString()) differ++;
    }
    CHECK(differ > 0);  // the flag does change the stream when on
}

TEST(arg_sentinel_reaches_its_targets_and_not_its_rulers) {
    // Section 113 predictions 1-3, at the piece level.
    CHECK_EQ(argSentinelHits(MajorityValueProblem(), false, 1), 0);
    CHECK_EQ(argSentinelHits(LongestRunValueProblem(), false, 2), 0);
    // majority_value is reachable but rare: 3 of 400000 draws (section 113
    // amendment), too slow for a unit test.
    CHECK(argSentinelHits(LongestRunValueProblem(), true, 2) > 0);
    CHECK(argSentinelHits(LongestIncreasingRunEndProblem(), true, 3) > 0);
    CHECK(argSentinelHits(FrequentModeOrNoneProblem(), true, 4) > 0);
    CHECK_EQ(argSentinelHits(LongestRunStartProblem(), true, 5), 0);
    CHECK_EQ(argSentinelHits(PluralityValueProblem(), true, 6), 0);
}
