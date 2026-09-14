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
