#pragma once

#include "ast/Stmt.hpp"
#include "core/Random.hpp"

namespace algosearch {

// Bounds respected while generating/growing programs, to avoid unbounded
// AST explosion during random construction.
struct GenConfig {
    int numVars = 4;
    int maxExprDepth = 3;
    int maxStatementsPerBlock = 4;
    int maxBlockDepth = 3;   // nesting depth of if/while
    int maxLoops = 2;        // max number of while-loops in a whole program
    int maxConst = 3;        // generated integer literals are in [0, maxConst]

    // Grammar-level bias, in [0, 1], toward directly emitting a canonical
    // "counting loop" skeleton --
    //   acc = 0 (or arr[0]);  i = 0;  while (i < n) { <body>; i = i + 1; }
    // -- instead of a fully unstructured random statement, whenever a loop
    // is being generated. <body> is itself one of a few canonical
    // accumulation shapes (running sum, running min/max via if, target
    // membership check, adjacent-swap) rather than arbitrary code, so the
    // *skeleton* that real array algorithms need (an index that starts at
    // 0, is compared against n, and increments exactly once) is handed to
    // the generator directly with higher probability, rather than left to
    // emerge from unbiased mutation alone.
    //
    // Default 0.4: the peak of a 160-run sweep (5 seeds x 4 problems x 8
    // bias values in [0, 0.7], see data/bias_map/ and
    // data/bias_map_summary.csv) -- combined generalization rate went
    // 1/20 (bias=0) -> 18/20 (bias=0.4) -> 17/20 (bias=0.7), a clear peak,
    // not a monotonic "more is better" curve. Pass 0.0 to fully restore
    // the original unbiased generator.
    double loopAccumulatorBias = 0.4;

    // Structural prior, layer 2 (independent of loopAccumulatorBias):
    // targets the two specific patterns the forensic AST analysis flagged
    // as discriminating generalizers from non-generalizers on their own,
    // rather than only via the fixed counting-loop template.
    //
    // When generating a generic assignment INSIDE a while body, with this
    // probability emit the self-increment idiom `v = v + 1` for a random
    // variable instead of a fully random expression -- every generalizing
    // min/max/sum program found so far contained this idiom (94.5%
    // overall, 100% for min/max/sum specifically; see
    // data/structural_features.csv), so nudging the *generic* generator
    // (not just the dedicated loop template) to produce it more often is
    // a direct test of whether that correlation is causal.
    //
    // Default stays 0.0 (opt-in): an A/B test at 0.3 (+ bareSwapPenalty
    // 0.3, on top of loopAccumulatorBias=0.4) moved combined
    // generalization only 17/20 -> 18/20 with mixed per-problem effects
    // (contains clearly better and faster; max/sum slower), and a
    // follow-up falsification test (this alone, loopAccumulatorBias=0.0,
    // strength up to 0.9) never rescued MinArrayProblem on its own --
    // flat 0/5 at every strength. The idiom alone isn't causally
    // sufficient; it has to co-occur with correct init and bound
    // generation, which is exactly what loopAccumulatorBias's atomic
    // template provides and this marginal per-statement nudge doesn't.
    // See data/prior_ab/ and data/falsification2/. Left available for
    // further experimentation, not promoted to a default.
    double indexIncrementBias = 0.0;

    // Reduces (never eliminates -- a prior should shrink probability, not
    // remove expressiveness) the chance of generating a swap OUTSIDE an
    // if's body, i.e. one not immediately guarded by a comparison.
    // Non-generalizers used swap almost twice as often as generalizers
    // (0.74 vs 0.39 mean occurrences) -- this tests whether that's because
    // ungated/unconditional swaps are a marker of unstructured search, not
    // whether swap itself is bad (a guarded compare-and-swap is exactly
    // what sorting needs, and stays at its original probability). Applied
    // as `effective swap chance = 0.5 - bareSwapPenalty` when not directly
    // inside an if; capped so at least a 0.05 residual chance remains.
    double bareSwapPenalty = 0.0;
};

// Generates a brand-new random program (a Block) with approximately
// `targetSize` AST nodes (generation stops once the size is reached or
// generation limits are hit, so actual size may be slightly smaller).
Program generateRandomProgram(Random& rng, int targetSize, const GenConfig& cfg);

// Clones `seed` and grows it by inserting freshly generated statements into
// randomly chosen blocks until its size reaches (at least) `targetSize`, or
// no more growth is possible. Used by beam search to expand promising
// candidates from one program size to the next.
Program growProgram(const Stmt& seed, int targetSize, Random& rng, const GenConfig& cfg);

}  // namespace algosearch
