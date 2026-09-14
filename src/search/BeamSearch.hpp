#pragma once

#include <cstdint>
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
    // reached and never re-broken), most recent last.
    std::vector<SolutionRecord> run();

private:
    const Problem& problem_;
    SearchConfig config_;
};

}  // namespace algosearch
