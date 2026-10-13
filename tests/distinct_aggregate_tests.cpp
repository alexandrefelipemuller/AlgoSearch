#include <string>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/EvenFrequencyValuesProblem.hpp"
#include "problems/MaxRepeatedValueProblem.hpp"
#include "problems/MinRepeatedValueProblem.hpp"
#include "problems/OddFrequencyValuesProblem.hpp"
#include "problems/SumDistinctProblem.hpp"
#include "problems/TripleFrequencyValuesProblem.hpp"
#include "problems/SumUniqueValuesProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// The current default's stateful family (section 112 adds one axis to DistinctCount).
GenConfig distinctProbe(bool aggregate) {
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.maxLoops = 4;
    cfg.loopAccumulatorBias = 0.0;
    cfg.combinatorBias = 1.0;
    cfg.combinatorState = true;
    cfg.combinatorFrequency = true;
    cfg.combinatorArgBest = true;
    cfg.combinatorDistinct = true;
    cfg.distinctAggregate = aggregate;
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

int distinctHits(const Problem& problem, bool aggregate, uint64_t seed, int draws = 40000) {
    Random rng(seed);
    GenConfig cfg = distinctProbe(aggregate);
    int hits = 0;
    for (int i = 0; i < draws; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        if (solvesLevels(problem, *p, 2, 6)) hits++;
    }
    return hits;
}

}  // namespace

TEST(section_112_problems_compute_their_answers) {
    CHECK_EQ(SumUniqueValuesProblem().answer({3, 1, 3, 4}), 5);
    CHECK_EQ(EvenFrequencyValuesProblem().answer({2, 2, 1, 5, 5, 5, 5}), 2);
    CHECK_EQ(MaxRepeatedValueProblem().answer({4, 1, 3, 1, 3}), 3);
    CHECK_EQ(MaxRepeatedValueProblem().answer({4, 1}), 0);
    CHECK_EQ(MinRepeatedValueProblem().answer({4, 1, 3, 1, 3}), 1);
    CHECK_EQ(MinRepeatedValueProblem().answer({4, 1}), -1);
    CHECK_EQ(TripleFrequencyValuesProblem().answer({2, 2, 1, 2, 3, 3, 3, 3}), 1);
}

TEST(distinct_aggregate_is_byte_identical_when_off) {
    GenConfig a = distinctProbe(false), b = distinctProbe(false), c = distinctProbe(true);
    Random ra(112), rb(112), rc(112);
    int differ = 0;
    for (int i = 0; i < 300; ++i) {
        std::string ta = generateRandomProgram(ra, 30, a)->toString();
        CHECK_EQ(ta, generateRandomProgram(rb, 30, b)->toString());
        if (ta != generateRandomProgram(rc, 30, c)->toString()) differ++;
    }
    CHECK(differ > 0);  // the flag does change the stream when on
}

TEST(distinct_aggregate_reaches_its_targets_and_not_its_rulers) {
    // Section 112d predictions 1-2, at the piece level.
    CHECK_EQ(distinctHits(SumDistinctProblem(), false, 1), 0);
    CHECK_EQ(distinctHits(OddFrequencyValuesProblem(), false, 2), 0);
    CHECK(distinctHits(SumDistinctProblem(), true, 1) > 0);
    CHECK(distinctHits(OddFrequencyValuesProblem(), true, 2) > 0);
    CHECK(distinctHits(SumUniqueValuesProblem(), true, 3) > 0);
    CHECK(distinctHits(EvenFrequencyValuesProblem(), true, 4) > 0);
    CHECK(distinctHits(MaxRepeatedValueProblem(), true, 5) > 0);
    CHECK_EQ(distinctHits(MinRepeatedValueProblem(), true, 7), 0);
    CHECK_EQ(distinctHits(TripleFrequencyValuesProblem(), true, 6), 0);
}
