#include "problems/MinArrayProblem.hpp"
#include "problems/SortingProblem.hpp"
#include "search/BeamSearch.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// A problem designed so the very first candidate the search tries (the
// empty program, deterministically generated at program size 1 since no
// generation loop iterations run) is a perfect "gambiarra": it solves
// level 1 by leaving the array untouched (output[0] == input[0] == 0 ==
// what level 1 requires), but can NEVER solve level 2, which requires
// output[0] == 99999 -- a constant far outside anything a tiny random
// program can plausibly produce. This makes the invariance gate's
// accept/reject decision deterministic for a unit test.
class GambiarraTrapProblem : public Problem {
public:
    std::vector<Instance> generateInstances(int level) const override {
        std::vector<int> input(static_cast<size_t>(level), 0);
        int required = (level == 1) ? 0 : 99999;
        return {Instance{input, {required}}};
    }
    std::string describeLevel(int level) const override { return "N = " + std::to_string(level); }
    int nextLevel(int level) const override { return level + 1; }
    bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const override {
        int required = (input.size() == 1) ? 0 : 99999;
        return !output.empty() && output[0] == required;
    }
    int numVars() const override { return 3; }
};

// A problem the empty/no-op program solves at every level (output[0] must
// be 0, and the no-op leaves the all-zero input untouched) -- used to
// verify that a genuinely generalizing program produces one row per level
// with a consistent programId/firstLevel/confirmedLevels.
class AlwaysZeroProblem : public Problem {
public:
    std::vector<Instance> generateInstances(int level) const override {
        return {Instance{std::vector<int>(static_cast<size_t>(level), 0), {0}}};
    }
    std::string describeLevel(int level) const override { return "N = " + std::to_string(level); }
    int nextLevel(int level) const override { return level + 1; }
    bool isSolution(const std::vector<int>&, const std::vector<int>& output) const override {
        return !output.empty() && output[0] == 0;
    }
    int numVars() const override { return 3; }
};

}  // namespace

TEST(champion_records_share_program_id_and_horizon_across_a_cascade) {
    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 5;
    cfg.stepLimit = 200;
    cfg.maxLevel = 5;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    // The no-op program solves every level from 1 to 5 in one shot, so
    // this must be a single cascade producing exactly 5 rows.
    CHECK_EQ(solutions.size(), static_cast<size_t>(5));
    const std::string& id = solutions.front().programId;
    CHECK(!id.empty());
    for (size_t i = 0; i < solutions.size(); ++i) {
        CHECK_EQ(solutions[i].programId, id);
        CHECK_EQ(solutions[i].firstLevel, 1);
        CHECK_EQ(solutions[i].confirmedLevels, 5);
        CHECK_EQ(solutions[i].levelReached, static_cast<int>(i) + 1);
        CHECK_EQ(solutions[i].finalStatus, std::string("SUCCESS"));
        CHECK(solutions[i].beamDiversity >= 1);
    }
}

TEST(diversity_rate_zero_reproduces_original_behavior) {
    SortingProblem problem(6, 20, 3, 99);
    SearchConfig cfg;
    cfg.beamWidth = 50;
    cfg.maxProgramSize = 8;
    cfg.stepLimit = 2000;
    cfg.maxLevel = 1;
    cfg.seed = 99;
    cfg.candidatesPerSize = 100;
    cfg.genConfig.numVars = 3;
    cfg.diversityRate = 0.0;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.back().score, solutions.back().totalTests);
}

TEST(diversity_rate_never_loses_the_best_candidate) {
    // With a large diversityRate, most of the beam is replaced by random
    // immigrants every round -- but the champion (best-by-score) must
    // never be one of the discarded slots, so a full solve should still
    // be found and reported just like with diversityRate == 0.
    SortingProblem problem(6, 20, 3, 99);
    SearchConfig cfg;
    cfg.beamWidth = 50;
    cfg.maxProgramSize = 8;
    cfg.stepLimit = 2000;
    cfg.maxLevel = 1;
    cfg.seed = 99;
    cfg.candidatesPerSize = 100;
    cfg.genConfig.numVars = 3;
    cfg.diversityRate = 0.5;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.back().score, solutions.back().totalTests);
}

TEST(invariance_gate_marks_a_one_level_fit_as_unconfirmed) {
    GambiarraTrapProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 20;
    cfg.maxProgramSize = 10;
    cfg.stepLimit = 500;
    cfg.maxLevel = 2;
    cfg.seed = 1;
    cfg.candidatesPerSize = 50;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 1;  // must also hold at level 2 to count as confirmed

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    // The empty/no-op program solves level 1 outright but can never solve
    // level 2 (needs a 99999 constant no small random program will hit).
    // Search progression still advances (so the engine keeps a chance at
    // finding something that DOES work at level 2), but the level-1 record
    // must be labeled unconfirmed rather than a validated SUCCESS.
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().levelReached, 1);
    CHECK_EQ(solutions.front().finalStatus, std::string("UNCONFIRMED"));
}

TEST(invariance_gate_disabled_falls_back_to_old_single_level_promotion) {
    GambiarraTrapProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 20;
    cfg.maxProgramSize = 10;
    cfg.stepLimit = 500;
    cfg.maxLevel = 2;
    cfg.seed = 1;
    cfg.candidatesPerSize = 50;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 0;  // gate disabled: original behavior

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    // Same gambiarra, same seed -- but without the gate it gets credited
    // (SUCCESS) for level 1 immediately, exactly like the pre-invariance-gate
    // engine would have.
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().levelReached, 1);
    CHECK_EQ(solutions.front().finalStatus, std::string("SUCCESS"));
}

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

// Islands (SearchConfig::islandGenConfigs / BeamSearch::runIslands): a
// single population under loopAccumulatorBias=0.0 is documented
// (FINDINGS.md section 5) to essentially never solve min/max/sum without
// the skeleton bias -- confirmed directly below at this tiny budget
// first, so the crippled config is genuinely crippled and not just
// "unlucky at this seed." Running the SAME crippled config as three of
// four islands, alongside one island with the working bias, should still
// find a solution: either that one working island finds it on its own
// (islands never disabled it), or a struggling island benefits from a
// migrated clone -- both are legitimate outcomes of enabling islands, and
// this test only asserts the end result the feature exists to provide,
// not which path produced it.
TEST(islands_disabled_single_population_with_zero_bias_finds_nothing_at_this_budget) {
    MinArrayProblem problem(6, 40, 4, 7);
    SearchConfig cfg;
    cfg.beamWidth = 30;
    cfg.maxProgramSize = 15;
    cfg.stepLimit = 2000;
    cfg.maxLevel = 2;
    cfg.seed = 7;
    cfg.candidatesPerSize = 100;
    cfg.genConfig.numVars = 4;
    cfg.genConfig.loopAccumulatorBias = 0.0;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();
    // N=1 is trivial enough that even a zero-bias population "solves" it
    // (any tiny random program can get lucky on a single-element array),
    // so `solutions` isn't necessarily empty -- what a zero-bias
    // population genuinely never reaches at this budget is N=2, which
    // needs an actual comparison. Confirmed here so the crippled config
    // used below is genuinely crippled, not just unlucky at this seed.
    for (const auto& rec : solutions) {
        CHECK(rec.levelReached < 2);
    }
}

TEST(islands_with_a_crippled_majority_still_solve_what_the_crippled_config_alone_cannot) {
    MinArrayProblem problem(6, 40, 4, 7);
    SearchConfig cfg;
    cfg.beamWidth = 30;
    cfg.maxProgramSize = 15;
    cfg.stepLimit = 2000;
    cfg.maxLevel = 2;
    cfg.seed = 7;
    cfg.candidatesPerSize = 100;
    cfg.genConfig.numVars = 4;
    cfg.migrationInterval = 3;
    cfg.migrationRate = 0.2;

    GenConfig crippled;
    crippled.numVars = 4;
    crippled.loopAccumulatorBias = 0.0;
    GenConfig working;
    working.numVars = 4;
    working.loopAccumulatorBias = 0.4;
    cfg.islandGenConfigs = {crippled, crippled, crippled, working};

    BeamSearch search(problem, cfg);
    auto solutions = search.run();
    CHECK(!solutions.empty());
    // Must reach N=2 (the level the crippled config alone never reaches,
    // per the test above) with a CONFIRMED full solve, not just inherit
    // the trivial N=1 record every config gets for free.
    CHECK(solutions.back().levelReached >= 2);
    CHECK_EQ(solutions.back().score, solutions.back().totalTests);
    // The winning island's index is recorded in the persisted config pairs
    // (see BeamSearch::runIslands) -- confirms the multi-island path, not
    // the single-population fallback, is actually the one that ran.
    bool foundWinningIslandKey = false;
    for (const auto& kv : solutions.back().config) {
        if (kv.first == "winning_island") foundWinningIslandKey = true;
    }
    CHECK(foundWinningIslandKey);
}

// islandGenConfigs with 0 or 1 entries must reproduce the original
// single-population run() path exactly (see the dispatch in
// BeamSearch::run()) -- a single-entry list is NOT routed through
// runIslands(), so this locks in that the size-based dispatch threshold
// (>1) behaves as documented and doesn't accidentally start treating a
// solo config list as "islands enabled."
TEST(a_single_island_config_falls_back_to_the_original_single_population_path) {
    SortingProblem problem(6, 20, 3, 5);
    SearchConfig cfg;
    cfg.beamWidth = 30;
    cfg.maxProgramSize = 6;
    cfg.stepLimit = 1000;
    cfg.maxLevel = 1;
    cfg.seed = 5;
    cfg.candidatesPerSize = 60;
    cfg.genConfig.numVars = 3;

    GenConfig solo = cfg.genConfig;
    cfg.islandGenConfigs = {solo};

    BeamSearch search(problem, cfg);
    auto solutions = search.run();
    CHECK(!solutions.empty());
    // Single-population path never writes a "winning_island" key.
    for (const auto& kv : solutions.back().config) {
        CHECK(kv.first != "winning_island");
    }
}
