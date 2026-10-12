#include <cstdio>
#include <fstream>
#include <set>

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

TEST(post_solve_sizes_zero_reproduces_single_cascade_records) {
    // Default mode: the very first full solve promotes immediately, so the
    // AlwaysZeroProblem (no-op solves every level) yields exactly one record
    // per level, all from the same champion.
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
    cfg.postSolveSizes = 0;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK_EQ(solutions.size(), static_cast<size_t>(5));
    const std::string& id = solutions.front().programId;
    for (const auto& rec : solutions) CHECK_EQ(rec.programId, id);
}

TEST(post_solve_sizes_keeps_level_open_and_records_alternative_champions) {
    // With a divergence window, the level stays open after the first full
    // solve, so further full-solve programs found at later sizes are also
    // recorded. The no-op champion is a SUCCESS; a window alternative may
    // also be recorded as UNCONFIRMED (solves N=1 but fails structural
    // invariance), which is the search labelling it, not an error. At least
    // one record per level must exist.
    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 5;
    cfg.stepLimit = 200;
    cfg.maxLevel = 3;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 3;
    cfg.postSolveSizes = 2;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().finalStatus, std::string("SUCCESS"));
    for (const auto& rec : solutions) CHECK(rec.finalStatus == "SUCCESS" || rec.finalStatus == "UNCONFIRMED");
    // The no-op champion's cascade covers levels 1..3 even with the window on.
    bool saw1 = false, saw2 = false, saw3 = false;
    for (const auto& rec : solutions) {
        if (rec.levelReached == 1) saw1 = true;
        if (rec.levelReached == 2) saw2 = true;
        if (rec.levelReached == 3) saw3 = true;
    }
    CHECK(saw1 && saw2 && saw3);
}

TEST(post_solve_sizes_window_still_promotes_after_the_budget_elapses) {
    // The window must not deadlock the search: after postSolveSizes extra
    // sizes, promotion happens and the run ends with the gambiarra still
    // labeled UNCONFIRMED (it never generalizes), exactly as in the
    // postSolveSizes=0 case.
    GambiarraTrapProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 20;
    cfg.maxProgramSize = 10;
    cfg.stepLimit = 500;
    cfg.maxLevel = 2;
    cfg.seed = 1;
    cfg.candidatesPerSize = 50;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 1;
    cfg.postSolveSizes = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().levelReached, 1);
    CHECK_EQ(solutions.front().finalStatus, std::string("UNCONFIRMED"));
}

TEST(checkpoint_path_writes_progress_before_the_run_completes) {
    // Long unattended runs have no internal wall-clock cutoff (FINDINGS.md
    // section 8) and previously only wrote solutions.json on natural
    // completion -- an external `timeout`/`kill` lost everything. With
    // checkpointPath set, every level promotion overwrites that path with
    // the full solutions vector accumulated so far, so a run killed mid-way
    // still has something on disk.
    std::string path = "/tmp/algosearch_checkpoint_test.json";
    std::remove(path.c_str());

    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 5;
    cfg.stepLimit = 200;
    cfg.maxLevel = 4;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 3;
    cfg.checkpointPath = path;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    std::ifstream in(path);
    CHECK(in.good());
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    CHECK(!content.empty());
    // The checkpoint is written at every promotion BEFORE the final level,
    // so it reflects at least the first level's record, though it can lag
    // one level behind the final in-memory result (no promotion follows the
    // very last level).
    CHECK(content.find("program_id") != std::string::npos);

    std::remove(path.c_str());
}

TEST(empty_checkpoint_path_writes_no_file) {
    // Default (empty checkpointPath) must reproduce today's behavior
    // exactly: no extra file, no extra I/O.
    std::string path = "/tmp/algosearch_checkpoint_test_unused.json";
    std::remove(path.c_str());

    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 5;
    cfg.stepLimit = 200;
    cfg.maxLevel = 4;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 3;
    // cfg.checkpointPath left empty (default)

    BeamSearch search(problem, cfg);
    search.run();

    std::ifstream in(path);
    CHECK(!in.good());
}

TEST(first_solve_at_max_level_is_unconfirmed_not_vacuously_success) {
    // A cascade that starts AT maxLevel has no level beyond it to check;
    // before the fix the capped lookahead passed trivially and labeled it
    // SUCCESS (FINDINGS.md section 37). AlwaysZeroProblem's no-op program
    // solves every level, so starting the search at maxLevel isolates the
    // case: the record must stay UNCONFIRMED.
    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 20;
    cfg.maxProgramSize = 5;
    cfg.stepLimit = 200;
    cfg.startLevel = 5;
    cfg.maxLevel = 5;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK_EQ(solutions.size(), static_cast<size_t>(1));
    CHECK_EQ(solutions.front().firstLevel, 5);
    CHECK_EQ(solutions.front().finalStatus, std::string("UNCONFIRMED"));
}

TEST(restart_on_stall_restarts_a_stuck_run_and_keeps_the_archive) {
    // Section 94: GambiarraTrapProblem solves level 1 at once and can never
    // solve level 2, so the run stalls there. With restartOnStall = 3 it
    // must start over from level 1 with a new seed, keep the level-1 record
    // across restarts, and still stop after maxProgramSize total steps.
    GambiarraTrapProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 20;
    cfg.stepLimit = 200;
    cfg.maxLevel = 4;
    cfg.seed = 7;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.restartOnStall = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK(search.restarts() >= 2);
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().levelReached, 1);
    CHECK_EQ(solutions.front().seed, static_cast<uint64_t>(7));
}

TEST(restart_on_stall_off_never_restarts) {
    GambiarraTrapProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 20;
    cfg.stepLimit = 200;
    cfg.maxLevel = 4;
    cfg.seed = 7;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK_EQ(search.restarts(), 0);
    CHECK_EQ(solutions.size(), static_cast<size_t>(1));
}

TEST(wall_seconds_stops_the_run_early) {
    // A wall budget already spent at the first size step stops the run
    // before anything is searched.
    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 5;
    cfg.stepLimit = 200;
    cfg.maxLevel = 5;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.wallSeconds = 1e-9;

    BeamSearch search(problem, cfg);
    CHECK(search.run().empty());
}

TEST(restart_on_stall_restarts_a_run_that_solves_every_level) {
    // Section 96: AlwaysZeroProblem is solved through maxLevel at once.
    // Without restarts the search ends; with them it starts a new run.
    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 6;
    cfg.stepLimit = 200;
    cfg.maxLevel = 3;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.restartOnStall = 100;

    BeamSearch search(problem, cfg);
    auto solutions = search.run();

    CHECK(search.restarts() >= 2);
    CHECK_EQ(solutions.size(), static_cast<size_t>(3));  // program@level dedup across runs
}

namespace {

// Section 97: every input value equals the problem's seed and output[0]
// must be 0, so the no-op program solves every level with seed 0 but fails
// every fresh instance built with any other seed -- a stand-in for a
// value-gated junk branch the fixed instance set never triggers.
class SeedValuedProblem : public Problem {
public:
    explicit SeedValuedProblem(int seed) : seed_(seed) {}
    std::vector<Instance> generateInstances(int level) const override {
        return {Instance{std::vector<int>(static_cast<size_t>(level), seed_), {0}}};
    }
    std::string describeLevel(int level) const override { return "N = " + std::to_string(level); }
    int nextLevel(int level) const override { return level + 1; }
    bool isSolution(const std::vector<int>&, const std::vector<int>& output) const override {
        return !output.empty() && output[0] == 0;
    }
    int numVars() const override { return 3; }

private:
    int seed_;
};

SearchConfig reseedConfig() {
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 2;
    cfg.stepLimit = 200;
    cfg.maxLevel = 5;
    cfg.seed = 1;
    cfg.candidatesPerSize = 10;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 3;
    return cfg;
}

}  // namespace

TEST(confirm_reseed_unconfirms_a_program_that_fails_fresh_seed_instances) {
    SeedValuedProblem problem(0), fresh(5);
    SearchConfig cfg = reseedConfig();
    cfg.confirmProblem = &fresh;
    cfg.confirmReseedLevels = 3;
    auto solutions = BeamSearch(problem, cfg).run();
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().finalStatus, std::string("UNCONFIRMED"));
}

TEST(confirm_reseed_keeps_success_when_fresh_instances_pass) {
    SeedValuedProblem problem(0), fresh(0);
    SearchConfig cfg = reseedConfig();
    cfg.confirmProblem = &fresh;
    cfg.confirmReseedLevels = 3;
    auto solutions = BeamSearch(problem, cfg).run();
    CHECK(!solutions.empty());
    CHECK_EQ(solutions.front().finalStatus, std::string("SUCCESS"));
}

TEST(profile_novelty_archives_fewer_alternative_champions) {
    // Section 98: every correct AlwaysZeroProblem program has constant cost,
    // so the gate drops alternative champions whose steps match an archived
    // one (8 -> 6 program ids here; tiny programs can still differ by > 1.5x).
    AlwaysZeroProblem problem;
    SearchConfig cfg;
    cfg.beamWidth = 10;
    cfg.maxProgramSize = 12;
    cfg.stepLimit = 200;
    cfg.maxLevel = 3;
    cfg.seed = 1;
    cfg.candidatesPerSize = 20;
    cfg.genConfig.numVars = 3;
    cfg.invarianceLookahead = 1;
    cfg.postSolveSizes = 8;

    auto plain = BeamSearch(problem, cfg).run();
    cfg.profileNovelty = 0.5;
    auto gated = BeamSearch(problem, cfg).run();

    std::set<std::string> plainIds, gatedIds;
    for (const auto& r : plain) plainIds.insert(r.programId);
    for (const auto& r : gated) gatedIds.insert(r.programId);
    CHECK(!gated.empty());
    CHECK(gatedIds.size() < plainIds.size());
}
