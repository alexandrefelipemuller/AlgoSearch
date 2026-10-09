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
    // Max number of while-loops in a whole program. Raised from 2 to 4:
    // heapsort (see genHeapSortSkeletonBlock in ProgramGenerator.cpp) is a
    // genuinely 4-loop shape -- a build-heap loop wrapping a sift-down
    // loop, then an extract-max loop wrapping its own sift-down loop --
    // needed to try for O(n log n) sorting at all (see FINDINGS.md
    // section 13's architecture discussion: Mini-C has no recursion and
    // only one array, ruling out merge/quicksort, so an in-place,
    // non-recursive shape is the only option, and heapsort is it). Purely
    // additive: raising the budget never forces any existing 1-2-loop
    // skeleton to use more loops than it already did, just makes room for
    // ones that need more.
    int maxLoops = 4;
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

    // Opt-in (0.0 = off, reproduces today's behavior exactly): probability,
    // when generating a GENERIC statement (not a dedicated skeleton) INSIDE
    // a while loop's body, of emitting `continue` or `break` instead. This
    // is Mini-C's missing early-exit primitive -- see
    // data/skeleton_middle_ground_experiments.md Test 4: every exact
    // branch-and-bound-style pruning algorithm needs a way to abandon the
    // rest of a loop iteration (or the loop itself) early, and until now
    // nothing in the grammar could express that at all, no matter how
    // biased generation got. Only ever emitted inside a while body (the
    // generator threads `insideLoop` through genStmt to enforce this), so
    // it can never appear as a structurally invalid orphan statement.
    // Split 70/30 continue/break when it fires: continue (skip to the next
    // candidate) is the shape branch-and-bound-style pruning actually
    // needs; break is included for completeness/symmetry at a lower rate.
    double controlFlowBias = 0.0;

    // Moção 2 (Etapa 3): composite-primitives operator, opt-in (0.0 = off,
    // reproduces today's behavior exactly). Probability, at a generic
    // statement slot with 2 free loop slots, of emitting a two-loop block
    // assembled from section 24's CATALOGUED primitives (captured scalar from
    // arr[0], capture at a computed position, two counting loops with derived
    // bounds, an affine two-sequence index, and a relation piece -- sentinel
    // flag / count-if / xor / running best) under ONE shared binding context:
    // every piece uses the context's variables, never fresh independent
    // draws. This is the binding discipline sections 7/10 showed the free
    // grammar lacks, and it is deliberately a FAMILY of composites (the
    // SubstringProblem solution is one draw among ~50, not the only thing the
    // operator can emit) rather than a new named skeleton. Test A of the
    // Etapa 3 protocol (FINDINGS.md) measures whether this reaches
    // `substring`; Test B (`lis`) must stay out of reach by construction.
    double compositeBias = 0.0;

    // Opt-in (0.0 = off, reproduces today's behavior exactly): probability
    // that growProgram, on top of its existing whole-STATEMENT point
    // mutation (which replaces an entire if/while -- condition, body, AND
    // else-body -- with a freshly generated one), also tries a finer-
    // grained EXPRESSION-level mutation: pick one existing if/while
    // somewhere in the tree and regenerate ONLY its condition, leaving its
    // body/else-body completely untouched. See
    // data/skeleton_middle_ground_experiments.md Test 3: a real gap found
    // while investigating why the branch-and-bound "hole" experiment never
    // refined a vacuous guard into a real one after its first random
    // draw -- the only existing mutation that could touch a guard replaces
    // the WHOLE enclosing if, destroying whatever correct logic lives
    // inside its body, so any such mutation is almost always fatal to a
    // candidate's score and never survives selection. This gives the
    // search a way to tweak a comparison in place without also blowing
    // away everything nested under it.
    double exprMutationRate = 0.0;

    // Opt-in (false = off, reproduces today's behavior exactly -- the
    // detectors below never touch the RNG): generation-time pruning of
    // statements/expressions that are trivial by their text alone -- `x = x`,
    // `arr[e] = arr[e]`, `swap(e, e)`, an if/while condition whose truth value
    // is constant (`v3 < v3`, `0 >= 2`; a constant-true while is a guaranteed
    // TIMEOUT), and a Div/Mod/Shl/Shr whose right operand is a literal that
    // always errors (`/ 0`). When one is drawn, only the offending part is
    // redrawn (bounded), so the rest of the grammar's distribution is
    // untouched. Detection runs regardless of this flag and is reported at
    // exit when ALGOSEARCH_DEBUG_PRUNE_COUNTS is set (FINDINGS.md section 33).
    bool pruneGeneration = false;

    // Opt-in (false = off, reproduces today's behavior exactly): role-typed
    // variables for the GENERIC grammar only (skeletons and the composite
    // family keep their own bindings). v0 is the read-only bound n, odd scratch
    // variables are indices, even ones are values; array positions come only
    // from index expressions (index var, literal, index +/- literal, n -
    // literal), index vars are only assigned starts/steps/the bound/another
    // index, and a condition compares either index-vs-bound/index or
    // value-vs-value, never across roles. Motivated by section 35's ~90%
    // behavioral duplication, mostly identical crashes (FINDINGS.md section 36).
    bool roleTypedVars = false;

    // Opt-in (0.0 = off, reproduces today's behavior exactly): probability
    // that a top-level generic statement slot emits a combinator-algebra
    // block instead -- a product of layout x loop x relation axes built only
    // from section 24's catalogued primitives, under one shared binding
    // context (idea B, FINDINGS.md section 38; generalizes compositeBias).
    double combinatorBias = 0.0;

    // The adjacent-pair loop-axis value (elem(i-1), elem(i)) in a single pass,
    // FINDINGS.md section 40. On by default since section 42 (no dilution on
    // any previously solved problem); false reproduces the frozen v1 family
    // of commit fb0e539 exactly (`--combinator-v1`). Only matters when
    // combinatorBias > 0.
    bool combinatorAdjacent = true;

    // Section 52, option (a) of section 50: two relation values for the
    // adjacent-pair loop -- BestStep (running min/max of elem(i) - elem(i-1),
    // initialized from the first pair when there is one, else 0) and
    // CountSegments (1 + the number of adjacent pairs satisfying a drawn
    // comparison). Opt-in (`--combinator-step`); false is byte-identical.
    bool combinatorStep = false;

    // Section 55: stateful pieces (RunBest, ResetSum, EarlierCellsBest,
    // PrefixSuffix -- see genStatefulBlock), a quarter of combinator blocks
    // when on. Default on since section 62 (`--no-combinator-state` off).
    bool combinatorState = false;

    // Section 71: nested aggregates (an inner count / forall / best
    // difference per outer element, fed into an outer best / count-if /
    // exists-if) on nested loops, and an absolute value on BestStep.
    // CLI default since section 72 (`--no-combinator-nested` turns it off);
    // false here is byte-identical, which the generator tests rely on.
    bool combinatorNested = false;

    // Section 73: the nested aggregate's outer test drawn from six canonical
    // tests on t >= 0 (t == 0, t > 0, t == 1, t > 1, t > n/2, t <= n/2)
    // instead of comparator x threshold. Opt-in
    // (`--nested-canonical-tests`); false is byte-identical.
    bool nestedCanonicalTests = false;

    // Section 75: a fifth stateful piece, FrequencyTable -- aux[i] = how many
    // j have cmp(elem(j), elem(i)) (frequency or rank), then one pass over
    // aux (max, min, count-if, exists). CLI default since section 76
    // (`--no-combinator-frequency` turns it off); false here is byte-identical.
    bool combinatorFrequency = false;

    // Section 77: pointer-following blocks (a walk p = arr[base + p] from a
    // start node, capped at v0 steps, ended by a pointer out of range) with
    // count-steps / still-in-range / visits-key / two-speed relations, one
    // combinator block in eight. Opt-in (`--combinator-follow`); false is
    // byte-identical.
    bool combinatorFollow = false;

    // Section 79: draw follow blocks only when the problem's inputs look like
    // pointer arrays (`pointerInput`, every value of every instance in
    // [0, length) -- set by BeamSearch from the instance data, never from the
    // problem's name). With the gate on and pointerInput false, generation is
    // byte-identical to combinatorFollow off. `--follow-pointer-inputs`.
    bool followPointerInputsOnly = false;

    // Section 80: difference direction (b - a or a - b) as an axis of every
    // difference-based relation (nested BestDiff, BestStep, the new
    // SumStep), and |step| on both step relations. CLI default since
    // section 81 (`--no-combinator-orthogonal` turns it off); false here is
    // byte-identical.
    bool combinatorOrthogonal = false;

    // Section 82: arg-best -- the plain Best, the nested outer Best and the
    // frequency table's max/min pass return where the best occurs (elem(i)
    // or i) instead of its value, half the time, with a tie rule (first,
    // last, or the smaller element wins). CLI default since section 85
    // (`--no-combinator-argbest` turns it off); false here is byte-identical.
    bool combinatorArgBest = false;

    // Section 87: a sixth stateful piece, DistinctCount -- per element, the
    // number of equal elements and whether one came earlier; only first
    // occurrences reach a threshold test, counted or tested for existence.
    // CLI default since section 88 (`--no-combinator-distinct` turns it off);
    // false here is byte-identical. (Replaces section 86's refuted
    // first-occurrence filter.)
    bool combinatorDistinct = false;
    bool pointerInput = false;

    // Section 66: skeletons are offered as shape only -- every site in the
    // hole grammar is drawn at random. Research hook for the section 66-68
    // probes (scripts/probe_shape_climb.cpp, scripts/sketch_bridge.cpp); no
    // CLI flag, since the approach was refuted. False is byte-identical.
    bool skeletonHoles = false;

    // Opt-in (0.0 = off, reproduces exactly today's fixed-bias behavior):
    // instead of leaving loopAccumulatorBias fixed for the whole run, let
    // BeamSearch nudge it each round toward the fraction of the current
    // beam's survivors that already contain the full atomic skeleton (init
    // + bound + increment together -- see hasCountingLoopSkeleton in
    // BeamSearch.cpp, the same causal unit identified in finding #7 of
    // FINDINGS.md, not a marginal per-statement pattern). This tests
    // whether adapting at runtime can match/beat a value found by a manual
    // 160-run sweep, and -- more informatively even if it doesn't beat it
    // here -- whether it converges to different steady-state values on
    // different problems, consistent with the per-problem peaks the sweep
    // found (contains peaked early/non-monotonically at 0.05; min/max
    // stayed saturated at higher values). See the design in FINDINGS.md's
    // adaptive-bias experiment section once run.
    double adaptiveSkeletonBiasRate = 0.0;
    // Floor: never let the adapted bias reach exactly 0 -- a prior should
    // shrink probability, not remove expressiveness (same principle as
    // bareSwapPenalty's residual floor).
    double adaptiveSkeletonBiasMin = 0.05;
    // Ceiling: avoid the beam collapsing onto skeleton-only generation.
    double adaptiveSkeletonBiasMax = 0.8;

    // loopAccumulatorBias's trigger also covers two more skeleton shapes,
    // picked uniformly at random alongside the flat counting-loop template
    // whenever it fires (see genStmt in ProgramGenerator.cpp):
    //
    //  - A nested "trial division by repeated subtraction" primality test
    //    (outer candidate-divisor loop, inner repeated-subtraction
    //    remainder loop) -- genCountingLoopBlock alone never produces two
    //    nested loops, so without this template, getting that shape to
    //    emerge by chance is the same kind of wall the sorting N=4 plateau
    //    hit before beam-width/candidates was raised. Needs 2 free loop
    //    slots.
    //  - Subtractive Euclidean GCD: a=arr[0]; b=arr[1];
    //    while ((a<b)+(a>b)) { if (a>b) a=a-b; else b=b-a; } arr[0]=a; --
    //    reachable at all only since genCondExpr gained the compound
    //    not-equal case, and even then a 10-minute run at generous
    //    beam-width/candidates never found it by chance (stuck flat at
    //    72/100 the whole time; see data/new_problems/).
    //
    // Language extension, added to test whether the "architecture ceiling"
    // discussed in FINDINGS.md (no recursion, no second array -> quicksort/
    // merge sort/timsort/etc. are inexpressible no matter how much search
    // budget or diversity is thrown at it) is really the bottleneck, or
    // whether the search just never needed to explore that far. Both knobs
    // default to 0.0 (fully opt-in, reproduces prior behavior exactly for
    // every problem that doesn't explicitly raise them) and, like
    // loopAccumulatorBias/the arithmetic extension, are deliberately raw
    // grammar primitives (a probability of substituting AuxAccess for
    // ArrayAccess, a probability of emitting a bare Call statement) rather
    // than a hand-authored quicksort/mergesort skeleton -- the point of the
    // experiment is to see what the free search does with the primitive
    // available, not to hand it the answer.
    //
    // Probability, wherever the generic statement generator would emit an
    // AssignArray (arr[i]=...), of emitting AssignAux (aux[i]=...) instead;
    // same substitution for ArrayAccess vs AuxAccess inside genValueExpr.
    // See ExprKind::AuxAccess / StmtKind::AssignAux.
    double secondArrayBias = 0.0;
    // Probability, at any generic-statement generation point, of emitting a
    // self-recursive Call(loExpr, hiExpr) instead. See StmtKind::Call and
    // Interpreter.cpp's reserved v1(lo)/v2(hi) convention. Requires
    // numVars >= 3 to be meaningful (silently has no effect below that,
    // same "never breaks, just inert" pattern as the other opt-in biases).
    double recursionCallBias = 0.0;

    // These are NOT separate opt-in flags one problem-by-problem run has
    // to be told to turn on (an earlier version of this file did exactly
    // that -- nestedDivisorLoopBias/gcdEuclidSkeletonBias CLI flags,
    // removed): that made the CLI usage itself reveal which algorithm a
    // given problem needs, which defeats the point of a general-purpose
    // synthesizer. A single, always-available trigger with the search's
    // own scoring deciding whether any given template pays off for
    // whatever problem is actually running is what makes this honestly
    // "universal" rather than a human routing pre-known answers in from
    // outside.
};

// Generates a brand-new random program (a Block) with approximately
// `targetSize` AST nodes (generation stops once the size is reached or
// generation limits are hit, so actual size may be slightly smaller).
// The TSP branch-and-bound skeleton with an arbitrary pruning guard in place
// of `cost < best` (FINDINGS.md section 41). `vars` are its 11 scalars in the
// skeleton's order: nv, size, uBase, chBase, pcBase, mBase, best, level, c,
// w, cost. `guardSite` is the If node whose condition is the guard.
struct TspBranchAndBound {
    StmtPtr program;
    const Stmt* guardSite = nullptr;
};
TspBranchAndBound buildTspBranchAndBoundWithGuard(const std::vector<int>& vars, ExprPtr guard);

// Section 63: section 22's stage-3 binary-search shape (direction detection,
// midpoint, found-check all hand-written) with the not-found decision given
// as `if (decision) { thenMove } else { elseMove }`. `vars` are target, lo,
// hi, mid, v, dir, found. `decisionSite` is that If node.
struct BinarySearchWithDecision {
    StmtPtr program;
    const Stmt* decisionSite = nullptr;
    const Stmt* foundCheckSite = nullptr;  // the `if (v == target)` node (or its stage-2 hole)
};
BinarySearchWithDecision buildBinarySearchWithDecision(const std::vector<int>& vars, ExprPtr decision, StmtPtr thenMove,
                                                       StmtPtr elseMove);
// Section 63, stage 2: the found-check is a hole too --
// `if (foundCheck) { foundBody } else { if (decision) { thenMove } else { elseMove } }`.
BinarySearchWithDecision buildBinarySearchStage2(const std::vector<int>& vars, ExprPtr foundCheck,
                                                 std::vector<StmtPtr> foundBody, ExprPtr decision, StmtPtr thenMove,
                                                 StmtPtr elseMove);

Program generateRandomProgram(Random& rng, int targetSize, const GenConfig& cfg);

// Clones `seed` and grows it by inserting freshly generated statements into
// randomly chosen blocks until its size reaches (at least) `targetSize`, or
// no more growth is possible. Used by beam search to expand promising
// candidates from one program size to the next.
Program growProgram(const Stmt& seed, int targetSize, Random& rng, const GenConfig& cfg);

// Subtree crossover between two independently-evolved programs: clones
// `recipient` and splices in a clone of a randomly chosen subtree from
// `donor` at a randomly chosen point. Unlike growProgram (which only ever
// inserts/replaces with a BRAND NEW random statement), this lets two beam
// lineages that each discovered a different piece of a solution combine
// them into one candidate -- the recombination operator classic genetic
// programming relies on and this search never had. See the rationale in
// ProgramGenerator.cpp.
Program crossoverPrograms(const Stmt& recipient, const Stmt& donor, Random& rng, const GenConfig& cfg);

}  // namespace algosearch
