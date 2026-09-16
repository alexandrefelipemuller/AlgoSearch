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

    // Where to write the per-round adaptive-bias CSV log (round, level,
    // bias_before, skeleton_rate, bias_after, best_score). Only written
    // when non-empty AND genConfig.adaptiveSkeletonBiasRate > 0.0 -- pure
    // diagnostics for the adaptive-bias experiment, never read back by the
    // search itself.
    std::string adaptiveBiasLogPath;
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

private:
    const Problem& problem_;
    SearchConfig config_;
};

}  // namespace algosearch
