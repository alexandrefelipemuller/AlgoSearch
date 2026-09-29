#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "problems/ContainsProblem.hpp"
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
bool familyReaches(const Problem& problem, uint64_t seed) {
    Random rng(seed);
    GenConfig cfg = combinatorOnly();
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
