#include "problems/SortingProblem.hpp"
#include "search/BeamSearch.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(beam_search_finds_solution_for_trivial_levels) {
    // N=1 is trivially solved by any program that doesn't corrupt the array
    // (including the empty program), so beam search must find a solution
    // quickly and report level 1 reached.
    SortingProblem problem(6, 20, 3, 99);

    SearchConfig cfg;
    cfg.beamWidth = 50;
    cfg.maxProgramSize = 8;
    cfg.stepLimit = 2000;
    cfg.startLevel = 1;
    cfg.maxLevel = 1;
    cfg.seed = 99;
    cfg.candidatesPerSize = 100;
    cfg.genConfig.numVars = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK(!solutions.empty());
    CHECK(solutions.back().levelReached >= 1);
    CHECK_EQ(solutions.back().score, solutions.back().totalTests);
}

TEST(beam_search_config_is_recorded_in_solution) {
    SortingProblem problem(6, 20, 3, 5);
    SearchConfig cfg;
    cfg.beamWidth = 30;
    cfg.maxProgramSize = 6;
    cfg.stepLimit = 1000;
    cfg.maxLevel = 1;
    cfg.seed = 5;
    cfg.candidatesPerSize = 60;
    cfg.genConfig.numVars = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.back().seed, static_cast<uint64_t>(5));
    CHECK(!solutions.back().config.empty());
}
