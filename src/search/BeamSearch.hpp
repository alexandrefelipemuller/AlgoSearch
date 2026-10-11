#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "problems/Problem.hpp"
#include "search/Persistence.hpp"
#include "search/ProgramGenerator.hpp"

namespace algosearch {

struct SearchConfig {
    int beamWidth = 200;
    int maxProgramSize = 30;
    long stepLimit = 10000;
    int startLevel = 1;
    int maxLevel = 5;
    uint64_t seed = 1234;
    int candidatesPerSize = 500;  // how many candidates to generate/score per program size
    GenConfig genConfig;
    unsigned numThreads = 0;  // 0 = auto-detect (std::thread::hardware_concurrency())

    // Structural invariance check: a full solve at the current level is
    // labeled a confirmed SolutionRecord (finalStatus "SUCCESS") only if
    // the SAME unmutated program also fully solves the next
    // `invarianceLookahead` levels; otherwise it's recorded as
    // "UNCONFIRMED" -- evidence the program fits this N's exact instance
    // family (a "gambiarra") rather than a real algorithm. This is
    // bookkeeping only: it never changes candidate generation/mutation,
    // and search progression (which level gets scored against next) always
    // advances on the cascade actually reached, confirmed or not --
    // blocking progression on an unconfirmed result was tried and starves
    // the search (it never gets to score against the harder level where a
    // genuinely generalizing program might be found; see BeamSearch.cpp).
    // 0 disables the check (every full solve is labeled SUCCESS, as before
    // this feature existed).
    int invarianceLookahead = 3;

    // Random immigrants: after ranking candidates by score/fitness for
    // this size step, the bottom `diversityRate` fraction of beamWidth
    // slots is replaced with brand-new random programs (same
    // generateRandomProgram() used everywhere else -- this changes beam
    // SELECTION, never candidate generation or mutation) instead of the
    // next-best-by-fitness candidates. The elite (top 1-diversityRate of
    // beamWidth) is always kept, so the current best is never lost. This
    // is the cheapest possible test of whether the beam is converging
    // prematurely onto one family of near-identical solutions: forcing a
    // steady trickle of unrelated fresh candidates into the population
    // costs nothing to implement and doesn't touch the search operator.
    // 0.0 (default) disables it and reproduces the original behavior.
    //
    // Kept at 0.0: an 80-run sweep (5 seeds x 4 problems x rates in
    // {0, 0.1, 0.2, 0.3}) with loop_bias disabled found random immigrants
    // never helped min/max/sum (flat 0/20 at every rate -- the bottleneck
    // turned out to be the generator's grammar, not population diversity;
    // see data/diversity_experiment/). A repeat of the same sweep WITH
    // loopAccumulatorBias=0.3 found 0.0 was the best rate overall (17/20)
    // and diversity_rate > 0 actively hurt SumArrayProblem (4/5 -> 2/5 at
    // 0.2-0.3); see data/diversity_experiment_with_bias/.
    double diversityRate = 0.0;

    // Crossover: in addition to growing each beam survivor in isolation
    // (growProgram, single-parent mutation only) and generating brand-new
    // random candidates, also generate `crossoverRate * candidatesPerSize`
    // candidates by picking two DISTINCT survivors from the current beam
    // and splicing a subtree from one into a clone of the other
    // (ProgramGenerator::crossoverPrograms). This is the one classic
    // genetic-programming operator this search never had: growProgram
    // alone means every beam lineage evolves in total isolation, so an
    // algorithm needing two or three independent structural insights (a
    // loop bound shape here, a comparison shape there) has to have ALL of
    // them land in the SAME lineage's random walk at once. Crossover lets
    // two lineages that each found a different piece combine them
    // directly. 0.0 (default) disables it and reproduces prior behavior
    // exactly. See FINDINGS.md once the crossover experiment is run for
    // whether it was kept on.
    double crossoverRate = 0.0;

    // Shape-niche elitism: reserves `shapeNicheRate * beamWidth` beam
    // slots for the single best candidate of each distinct structural
    // "shape" (see structuralShape() in BeamSearch.cpp -- counts of each
    // statement kind plus max while-nesting, already computed every round
    // for the beam_diversity diagnostic, just never acted on before this)
    // not already covered by the pure top-score elite core. Different
    // from diversityRate (which injects brand-new RANDOM programs,
    // i.e. noise): this preserves the actual BEST candidate of each
    // structurally-different lineage already found, so a lineage that is
    // genuinely progressing toward a correct structure (but currently
    // scores lower than some other, unrelated family that got a lucky
    // high score by coincidence rather than real partial progress -- see
    // the ContainsProblem "v0 rewrite" and gcd's "guess b" gambiarras
    // this project found) doesn't get crowded out of the beam and lost.
    // This is the classic evolutionary-computation "niching" fix for
    // premature convergence onto a single deceptive local optimum. 0.0
    // (default) disables it and reproduces prior behavior exactly (pure
    // top-K-by-score, same as always).
    double shapeNicheRate = 0.0;

    // Simulated-annealing walker temperature (0.0 = disabled). Maintains
    // one persistent solution, mutated locally each round via
    // growProgram, and accepted as the new state -- even when it scores
    // WORSE -- with Boltzmann probability exp(fitnessDelta/temperature),
    // where fitnessDelta is normalized by instance count so the same
    // temperature value is roughly comparable across problems of
    // different instance-set sizes. Temperature cools linearly across the
    // program-size sweep (this value at size=1, down to a small floor
    // near maxProgramSize), same idea as real annealing schedules. Unlike
    // diversityRate (fresh random restarts, unrelated to current
    // progress) or shapeNicheRate (never accepts a worse candidate), this
    // is a genuine controlled downhill walk that can cross a fitness
    // valley to escape a local optimum a pure-elitist beam can never
    // leave once it's fully converged onto one family. The walker's
    // current candidate is injected into the same scored pool every
    // round, so the elitist beam can adopt it the moment it becomes
    // competitive. 0.0 (default) disables it and reproduces prior
    // behavior exactly.
    double annealingTemperature = 0.0;

    // Behavior-niche elitism: reserves `behaviorNicheRate * beamWidth`
    // beam slots for the best candidate of each distinct BEHAVIOR
    // signature -- which exact instances it gets right, not what its code
    // looks like (see behaviorSignature() / CandidateRecord::correctness
    // in BeamSearch.cpp). This is the legitimate version of "reward
    // partial structural progress": unlike a hand-picked bonus for a
    // specific AST shape (which would just be a disguised way of telling
    // the search what a known-in-advance correct algorithm looks like),
    // rewarding behavioral distinctness never looks at what "correct"
    // means for a given problem or what a solution should look like --
    // it only compares which instances matched, which is exactly the
    // "novelty search" technique from the evolutionary-computation
    // literature (Lehman & Stanley) for escaping deceptive fitness
    // landscapes where raw score gives no useful gradient. Complementary
    // to shapeNicheRate (AST-shape diversity) -- this is
    // output-behavior diversity instead, a different and more directly
    // decision-relevant signal. 0.0 (default) disables it and reproduces
    // prior behavior exactly.
    double behaviorNicheRate = 0.0;

    // Score-band niching: reserves `scoreBandRate * beamWidth` beam slots
    // for the best candidate of each distinct SCORE BAND (score bucketed
    // into kScoreBands deciles of the instance count), scanning the
    // ranking downward so each band contributes its own best member.
    //
    // Motivated by a direct measurement, not a hypothesis. Probing the
    // mod problem's true solution (v1=arr[0]; v2=arr[1];
    // while (v1 > v2-1) { v1 = v1-v2; } arr[0]=v1) showed it scores
    // 100/100 while EVERY program one mutation away from it scores <= 73
    // -- and the plateau the search reaches unaided is 79. The correct
    // answer therefore sits on an isolated peak whose entire
    // neighbourhood is strictly below the incumbent, so a purely elitist
    // beam culls exactly the 72-73 candidates that are a single mutation
    // from 100. Several of those neighbours differ from the solution by
    // one variable index, so the final step is genuinely reachable -- the
    // search just never gets to keep a program standing on it.
    //
    // The four mechanisms already tried (diversityRate, shapeNicheRate,
    // behaviorNicheRate, annealingTemperature) all diversify within the
    // top score tier, which is why they failed identically here: none of
    // them preserves a lineage whose score is well below the best. This
    // one deliberately does, and is still problem-agnostic -- it knows
    // nothing about any specific problem, it only refuses to discard
    // fitness diversity. 0.0 (default) disables it and reproduces prior
    // behavior exactly.
    double scoreBandRate = 0.0;

    // Speed-aware fitness tie-break (data/skeleton_middle_ground_experiments.md,
    // "Tests 2 and 4" follow-up hypothesis): `score` (exact-correctness
    // count, used for promotion/labeling) is NEVER affected by this -- it
    // only adds `speedBonusWeight * (1 - steps_used/stepLimit)` per
    // SUCCESS instance to `fitness`, the search-internal ranking-only
    // field already used to break ties between equally-scoring candidates
    // (see CandidateRecord::fitness). Without this, two candidates that
    // solve the same instances identically-right/wrong are fitness-tied
    // regardless of how many interpreter steps either one took, so nothing
    // ever rewards a genuinely faster (still 100% correct) algorithm over
    // a slower one that happens to reach the same score first -- the
    // mechanism Test 2 found responsible for evolution settling on a
    // vacuous always-true pruning guard (correct, no speedup) instead of a
    // real one (correct AND faster): both scored identically, so nothing
    // ever preferred the real one. 0.0 (default) disables it and
    // reproduces prior behavior exactly.
    double speedBonusWeight = 0.0;

    // Moção 1 (Etapa 2 of the approved 3->1->2 plan): divergence window after
    // the first full solve of a level. 0 (default) reproduces the original
    // behavior exactly: the instant any candidate fully solves the current
    // level, its cascade is recorded and the search promotes. N > 0: after
    // that first full solve the level stays open for N more program-size
    // steps; the champion is frozen (excluded from the beam so the elite
    // slots grow alternatives instead of re-selecting it), every NEW full-
    // solve candidate found during the window is recorded with its own
    // cascade, and promotion happens only when the window closes -- to the
    // level reached by the archived champion whose cascade went farthest
    // (ties broken by smaller program, then by fitness so an enabled
    // speedBonusWeight can prefer the faster equal-score champion). This is
    // the engine prerequisite for the held-out discovery benchmark
    // (FINDINGS.md section 25): it turns "found one correct program" into
    // "keep looking for smaller/faster/different correct programs".
    int postSolveSizes = 0;

    // Section 51: enumerate a small output-writing tail (TailFiller) after
    // the top `tailEnumTop` beam survivors at every size step. 0 = off,
    // byte-identical to before.
    int tailEnumTop = 0;
    // Section 89: from the second level on, every level's scoring set also
    // holds the first `regressionGuard` instances of each earlier level, so
    // a program that breaks smaller n loses score instead of only being
    // caught by the verifier. 0 = off, byte-identical.
    int regressionGuard = 0;

    // Where to write the per-round adaptive-bias CSV log (round, level,
    // bias_before, skeleton_rate, bias_after, best_score). Only written
    // when non-empty AND genConfig.adaptiveSkeletonBiasRate > 0.0 -- pure
    // diagnostics for the adaptive-bias experiment, never read back by the
    // search itself.
    std::string adaptiveBiasLogPath;

    // Opt-in (empty = off, reproduces today's behavior exactly): path to
    // overwrite with the FULL solutions vector so far every time the
    // search promotes to a new level. Exists for long unattended runs
    // (the engine has no internal wall-clock cutoff -- FINDINGS.md section
    // 8 -- and normally only writes solutions.json on natural completion,
    // via main.cpp's saveSolutions call after run() returns; a run killed
    // by an external wall timeout before that point loses everything). A
    // periodic checkpoint here means `kill`/`timeout` at any moment still
    // leaves the last-promoted level's solutions on disk. Cheap: writes at
    // most once per level, not per candidate or per size step.
    std::string checkpointPath;

    // Section 94: restart on stall. When > 0, a run whose highest level
    // reached has not risen for this many consecutive size steps starts
    // over from startLevel with an empty beam and a fresh search seed drawn
    // from a master stream seeded by `seed` (each run's seed is logged and
    // stored in its SolutionRecords). The archive persists across restarts.
    // Counts divergence-window steps too, so it must exceed postSolveSizes.
    // A run that solves past maxLevel also restarts (section 96), so the
    // search then ends only on maxProgramSize or wallSeconds.
    // maxProgramSize then bounds total size steps across all restarts.
    // 0 = off, byte-identical.
    int restartOnStall = 0;
    // Stop cleanly (archive kept, normal exit) once this much wall time has
    // elapsed. 0 = no limit, as before.
    double wallSeconds = 0.0;
    // Section 97: when set and confirmReseedLevels > 0, SUCCESS also needs
    // the instances of the first confirmReseedLevels levels of this
    // problem built with another seed (not owned). nullptr/0 = off.
    const Problem* confirmProblem = nullptr;
    int confirmReseedLevels = 0;
};

// Beam Search over the space of Mini-C programs for a given Problem.
//
// Grows programs from very small ASTs up to `maxProgramSize`, scoring each
// by the number of correctly solved instances of the current problem level
// (score = correctness count only, never mixed with program size). Whenever
// a candidate fully solves a level, the problem is promoted to harder
// levels immediately, using the winning program as long as it keeps solving
// each next level completely.
class BeamSearch {
public:
    BeamSearch(const Problem& problem, SearchConfig config);

    // Runs the search; returns every solution found (one per level fully
    // reached and never re-broken), most recent last. When `outPrograms`
    // is non-null, it receives one AST clone per returned record, same
    // index for index -- SolutionRecord only keeps the program as text
    // (for JSON persistence), but callers that need to actually execute a
    // discovered program again (e.g. the benchmark tool) need the AST.
    std::vector<SolutionRecord> run(std::vector<Program>* outPrograms = nullptr);
    // Restarts the last run() performed (SearchConfig::restartOnStall).
    int restarts() const { return restarts_; }

private:
    const Problem& problem_;
    SearchConfig config_;
    int restarts_ = 0;
};

}  // namespace algosearch
