#include <string>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/SumAbsFromFirstProblem.hpp"
#include "problems/SumAbsFromLastProblem.hpp"
#include "problems/SumExcessOverFirstProblem.hpp"
#include "problems/SumOfDropsProblem.hpp"
#include "problems/SumOfRisesProblem.hpp"
#include "problems/SumSquaredStepsProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// The current default's combinator family (section 111 adds one axis to it).
GenConfig transformProbe(bool transform) {
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.maxLoops = 2;
    cfg.loopAccumulatorBias = 0.0;
    cfg.combinatorBias = 1.0;
    cfg.combinatorStep = true;
    cfg.combinatorNested = true;
    cfg.combinatorOrthogonal = true;
    cfg.combinatorTransform = transform;
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

int transformHits(const Problem& problem, bool transform, uint64_t seed, int draws = 20000) {
    Random rng(seed);
    GenConfig cfg = transformProbe(transform);
    int hits = 0;
    for (int i = 0; i < draws; ++i) {
        Program p = generateRandomProgram(rng, 2, cfg);
        if (solvesLevels(problem, *p, 2, 6)) hits++;
    }
    return hits;
}

}  // namespace

TEST(section_111_problems_compute_their_sums) {
    CHECK_EQ(SumExcessOverFirstProblem().answer({2, 5, 0, 3}), 4);
    CHECK_EQ(SumOfDropsProblem().answer({1, 4, 2, 3, 0}), 5);
    CHECK_EQ(SumSquaredStepsProblem().answer({1, 4, 2}), 13);
    CHECK_EQ(SumAbsFromLastProblem().answer({2, 5, 0, 3}), 6);
    CHECK_EQ(SumAbsFromLastProblem().answer({}), 0);
}

TEST(transform_piece_is_byte_identical_when_off) {
    GenConfig a = transformProbe(false), b = transformProbe(false);
    GenConfig c = transformProbe(true);
    Random ra(111), rb(111), rc(111);
    int differ = 0;
    for (int i = 0; i < 300; ++i) {
        std::string ta = generateRandomProgram(ra, 30, a)->toString();
        CHECK_EQ(ta, generateRandomProgram(rb, 30, b)->toString());
        if (ta != generateRandomProgram(rc, 30, c)->toString()) differ++;
    }
    CHECK(differ > 0);  // the flag does change the stream when on
}

TEST(transform_piece_reaches_its_targets_and_not_its_rulers) {
    // Section 111 predictions 1-3, at the piece level (known solutions exist
    // for all four targets in this family; none for the two rulers).
    CHECK_EQ(transformHits(SumOfRisesProblem(), false, 1), 0);
    CHECK_EQ(transformHits(SumAbsFromFirstProblem(), false, 2), 0);
    CHECK(transformHits(SumOfRisesProblem(), true, 1) > 0);
    CHECK(transformHits(SumAbsFromFirstProblem(), true, 2) > 0);
    CHECK(transformHits(SumExcessOverFirstProblem(), true, 3) > 0);
    CHECK(transformHits(SumOfDropsProblem(), true, 4) > 0);
    CHECK_EQ(transformHits(SumSquaredStepsProblem(), true, 5), 0);
    CHECK_EQ(transformHits(SumAbsFromLastProblem(), true, 6), 0);
}
