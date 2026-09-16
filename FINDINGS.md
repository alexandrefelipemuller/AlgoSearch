# Findings log

This file summarizes what has actually been learned running this project,
in the order it was learned, with pointers to the raw data backing each
claim. It exists so a new session (human or Claude) doesn't have to
re-derive context that's already been paid for. Update it when an
experiment produces a real conclusion; don't restate obvious code
behavior that's already documented at the call site.

## 1. V1 worked, but "solved" needs care

Beam search over Mini-C programs can discover real sorting algorithms for
small, exhaustively-tested N (found solutions for N=1..6 sorting,
exhaustive permutations). But a program that scores 100% on one N's exact
instance family can be a total non-generalizer -- see "generalization
horizon" below. Passing a level's tests is not proof of correctness for
all inputs; this was true from the start and got confirmed hard by
experiment.

## 2. Parallelism and a real memory bug

Scoring is embarrassingly parallel (each candidate vs. each instance is
independent) -- `BeamSearch::scoreCandidates` splits it across
`std::thread`, ~3x wall-clock speedup on 8 cores, deterministic results
(RNG stays single-threaded).

Real bug found and fixed: `Dedup` used to store full canonical program
text forever, unbounded. A run with max-program-size=2000 grew to >10GB
RSS and had to be killed mid-run. Fixed by storing a 64-bit hash instead
of the string, plus a hard cap (`maxEntries`, default 2M) that resets the
set instead of growing past it.

## 3. Sorting overfits badly on a naive instance set

`SortingProblem::generateInstances()` originally only produced
permutations of `{0..n-1}`. A program can hit 100% on that by exploiting
that the *values themselves* are the correct target indices -- without
doing anything like comparison-based sorting. Confirmed directly: the
"solutions" for N>=2 scored ~100% exact / ~30-50% on adversarial
instances (duplicates, negatives, wide range, reversed, sorted).

Fix: `generateInstances()` now folds in `generateStructuralInstances()`
(duplicates/negatives/sorted/reversed/all-equal/wide-range) for n>=2, so
passing a level requires generalizing past the permutation-shape
loophole. `generateAdversarialInstances()` uses a *different* seed offset
for a genuinely held-out post-hoc check (never used for scoring/
promotion).

## 4. Generalization horizon > single SUCCESS/FAIL

Every level-promotion event now records one row per level actually
reached, not just the final one, each with: `program_id` (stable hash),
`first_level` (where this program was found), `confirmed_levels` (the
generalization horizon -- how many consecutive levels the SAME
unmutated program held), plus training/held-out scores. See
`SolutionRecord` in `src/search/Persistence.hpp`.

**Structural invariance gate** (`SearchConfig::invarianceLookahead`,
default 3): a level is only labeled a confirmed `SUCCESS` if the same
program also holds `invarianceLookahead` levels ahead; otherwise
`UNCONFIRMED`. Important pitfall found and fixed: an early version
*blocked* level progression on an unconfirmed result -- this deadlocked
the search on EVERY problem tested, including one (`contains`) known to
generalize perfectly, because blocking progression means the engine
never gets to score against the harder level where the real solution is
findable. Fixed: progression always advances on the cascade reached;
the gate only changes the record's label, never search dynamics.

## 5. The core discovery: grammar prior >> population diversity

**The headline result of this project so far.**

Four step-back problems (`min`, `max`, `sum`, `contains` -- simpler than
sorting, same unmodified search engine) were used to isolate the
bottleneck for algorithm discovery.

Without any bias, min/max/sum essentially never generalize (0/20 each
across 20 independent seeds); `contains` generalizes ~9/20 (45%,
corrected after expanding from an initial unlucky 1/5 sample -- always
check more seeds before trusting a rate estimate).

**Diversity experiment** (`diversity_rate` = fraction of the beam
replaced by brand-new random "immigrant" programs each round,
independent of fitness): raising it up to 30% measurably increased beam
structural diversity 3.5x (130 -> 448 mean distinct shapes) but had
**zero** effect on min/max/sum's generalization rate (flat 0/60 combined
across all rates). Diversity was not the bottleneck.

**Grammar bias experiment** (`GenConfig::loopAccumulatorBias`: directly
emit a canonical `i=0; while(i<n){...; i=i+1;}` skeleton with probability
p instead of a fully random statement, whenever a loop would be
generated): a 160-run sweep (5 seeds x 4 problems x 8 bias values, see
`data/bias_map/` + `data/bias_map_summary.csv`) found a clear peak at
**bias=0.4** (18/20 combined, vs 1/20 at bias=0) -- not monotonic, a real
optimum. min went from 0/20 to 20/20. Per-problem curves diverge
interestingly: min/max saturate cleanly and stay saturated; `contains`
peaks early (bias=0.05, 5/5) and is non-monotonic, consistent with its
solution needing only a light nudge rather than a strong one. **This is
now the default** (`loopAccumulatorBias = 0.4`).

Conclusion to hold onto: **exploration, representation, and prior are
three different things.** The 80-run diversity experiment attacked
exploration and found nothing. The bias experiment attacked the prior
(what's *probable* to generate, not what's *possible*) and fixed three
problems that 160 prior runs across two other mechanisms never solved
once.

## 6. Forensic structural analysis: why the bias works

Parsed all 443 unique programs from the bias-map sweep (deterministic
Mini-C pretty-printer -> tree, see the analysis script's logic folded
into `data/structural_features.csv`) and compared generalizers (n=109)
vs. non-generalizers (n=334):

- `has_increment_pattern` (`v=v+1` idiom): **94.5% of generalizers, 37.7%
  of non-generalizers** -- 100%/100%/100% for min/max/sum specifically,
  zero exceptions across 80 programs. The single strongest discriminator
  found.
- More `while` (2.1x), more `while` directly containing an `if` (1.9x),
  deeper nesting (1.9x), more distinct variables (1.9x).
- **Fewer** swaps in generalizers (0.39 vs 0.74) -- non-generalizers lean
  on swap-based index tricks where a real algorithm would just assign;
  the earlier qualitative "swap madness" finding in overfit sort/min
  programs, now quantified.
- Loop position: generalizers' loop sits earlier/more centrally in the
  program (mean position 0.60 vs 0.82 on a 0=first/1=last scale) -- it's
  the backbone, not a bolted-on afterthought.

## 7. A single feature isn't independently causal -- the skeleton is

Two follow-up experiments, in order:

**A/B** (`loop-bias=0.4` alone vs. `+indexIncrementBias=0.3
+bareSwapPenalty=0.3`): small, mixed win (17/20 -> 18/20). `contains`
improved on both generalization and speed (2/5->4/5, ~2.4x faster,
plausibly because the swap penalty suits its comparison-guarded
solution); min/sum produced smaller programs without more successes;
max/sum got *slower*. Not the clean "same power, less time" result
hoped for -- see `data/prior_ab/`.

**Falsification** (critical, and the more interesting result): does
`indexIncrementBias` alone -- with `loopAccumulatorBias=0.0`, i.e. the
atomic skeleton template fully disabled -- rescue `min`? Tested at
strength 0.0/0.3/0.6/0.9, 5 seeds each. **Flat 0/5 at every strength**,
see `data/falsification2/`. The increment idiom correlating with success
(finding #6) does NOT mean marginally upweighting that one ingredient is
causally sufficient. What `loopAccumulatorBias` actually provides is
init (`i=0`), bound (`i<n`), and increment (`i=i+1`) **atomically, as one
coordinated unit** -- upweighting any single ingredient without raising
the joint probability of the other two doesn't compose into a working
loop. The causal unit is the skeleton, not any one statement pattern
within it.

Practical upshot: `indexIncrementBias`/`bareSwapPenalty` stay **off by
default** (mixed evidence, not a clean win) while `loopAccumulatorBias`
is on by default (clean, large, replicated win). Future grammar-prior
work should bias *composite patterns* (a template, like
`loopAccumulatorBias` already is), not marginal per-statement
probabilities.

## 8. Sorting: promising, but budget-mismatched so far

A single well-resourced run (loop-bias=0.3, max-program-size=500, 682s,
no time cutoff) reached N=1-4 all **confirmed** with perfect 13/13
held-out generalization -- better than any pre-bias sorting result in
this project. It plateaued at N=5 (123/128) for the rest of the budget.

At the standard 300s-per-run budget used for min/max/sum/contains
comparisons, 4/5 sorting seeds timed out mid-search without producing
any output at all (`solutions.json` is only written on normal
completion). Sorting's instance count grows factorially with N (128 at
N=5, 728 at N=6) where the other problems stay small/linear, so equal
wall-clock budgets are not a fair comparison across problems --
sorting needs either more time or a smaller max-n ceiling matched to
what 300s can actually afford. Not yet run as a proper multi-seed sweep
at a matched budget.

## 9. Native Le/Ge/Ne close the mod/gcd "wall" -- at least once, honestly

Mini-C previously had only `Lt`/`Gt`/`Eq` as native comparisons; `!=` was
emulated as `(x<y)+(x>y)` (added to close a representational gap -- see
`ProgramGenerator.cpp`'s old comment, now removed), and `<=`/`>=` had no
generic emulation at all in the random grammar (only hand-written
skeletons used ad hoc rewrites like `Lt(i, n+1)` for `i<=n`). Added `Le`,
`Ge`, `Ne` as three more native `ExprKind`s (`ast/Expr.hpp`), evaluated
directly in the interpreter, printed as `<=`/`>=`/`!=`, and sampled
uniformly against `Lt`/`Gt`/`Eq` in `genCondExpr` (1/6 each, replacing the
old 3/4 Lt/Gt/Eq + 1/4 compound-Ne split). This is a base grammar change,
not an opt-in bias knob, so the bar to keep it is "does it ever actually
help, without hurting anything else" -- not "is it a clean win everywhere."

**A/B, matched seeds (1-8), no skeleton (`loop-bias=0.0`), 180s wall-clock
per run, `mod`/`gcd`/`is_prime`, old grammar vs. new:** `gcd` and
`is_prime` came out byte-identical across every seed in both arms (63/100
and 7/11 respectively, no exceptions) -- for these two problems, in this
budget, the extra operators changed nothing. `mod` is where it mattered:
seed 7 hit **100/100** with the new grammar (old grammar's seed 7: 72/100).
Independently re-verified via `--benchmark` (the adversarial magnitude-shift
check in `Benchmark.cpp`, not just the training/held-out score) -- kept as
a genuinely generalizing solution, not rejected, `O(n)` complexity fit,
`confirmed_levels=30` (held at every level advanced through, N=1..30+).
Program is large (231 nodes) and structurally messy -- not a clean textbook
mod algorithm, more like an overfit-shaped blob that happens to be exactly
correct -- but it earns the 100/100 fair and square: same search engine,
same budget, only the grammar changed, and it passes the same independent
generalization check every other confirmed solution in this project goes
through.

No regression: `make test` 49/49 after the change; `min`/`max`/`sum`/
`contains`/`sorting` all score identically to the old grammar across 5
matched seeds each at the standard comparison budget (`contains` seed 5
advanced to a bigger level under the new grammar, not a regression -- it's
still 100% correct at whatever level it reached).

Conclusion: kept as the new default. One seed breaking through where six
prior generic mechanisms (diversity, crossover, shape/behavior-niche,
score-band niching, annealing) all failed identically on the same wall is
real signal that the missing native comparisons -- not selection pressure
-- were part of what walled gcd/mod off, even though gcd itself didn't
budge in this particular A/B. Not proof the wall is fully gone (`gcd` and
`is_prime` are still stuck, and `mod`'s other 7/8 seeds didn't reach
100/100 either), but a genuine partial win on a base-grammar change,
worth keeping and worth revisiting with more seeds/budget on `gcd`
specifically.

**Follow-up, same day:** a full 10-problem sweep at the *default*
loop-bias=0.4 (skeleton ON, not the no-skeleton isolation setting used
for the A/B above) -- modest budget (candidates=1500, max-program-size=150,
5 benchmark seeds, 240s wall cap/problem, see
`scripts/run_full_battery_modest.sh`, output in
`data/modest_battery_20260916_1135/`) -- found `gcd` solves **cleanly**
with the skeleton bias on: subtractive Euclidean GCD, confirmed through
level 20, a genuinely clean textbook algorithm (not a bloated blob like
the mod one above):
```
v2 = arr[0]; v1 = arr[1];
while ((v2<v1)+(v2>v1)) {           // != , via the compound form (native Ne exists but wasn't drawn here)
  if (v2>v1) v2=v2-v1; else v1=v1-v2;
}
arr[0] = v2;
```
So the "gcd wall" documented above was specifically a **no-skeleton**
phenomenon -- with `loopAccumulatorBias=0.4` (this project's actual
default, not the isolation setting used to test the grammar change in
isolation) plus native Le/Ge/Ne, gcd is not stuck at all. 9/10 problems in
this sweep produced independently-verified generalizing solutions
(sorting O(n^2), min/max/sum/contains/contains_ordered/fibonacci all
O(n), gcd O(n), is_prime O(n) with 6/9 candidates surviving the
adversarial recheck, 3 correctly rejected); `factorial` alone timed out
mid-search at the 240s cap, stuck at 43/45 on level 12 -- looked like a
budget shortfall (matches section 8's point that different problems need
different wall-clock budgets under equal settings), but turned out to be
CPU contention from running in the same sequential battery, not an
actual compute need: re-run isolated with the same settings and a 1800s
cap (`data/factorial_deep_20260916/`), it finished in ~35s per seed, all
5 seeds converging on the same genuinely-generalizing O(n^2) factorial
(nested-loop multiply-by-repeated-addition), 0 rejected. The 43/45
plateau at level 12 is real but benign -- factorial(27) overflows `int`,
so those 2 wrong instances are a numeric-range limit of the *problem*,
not a search failure; the algorithm itself is correct. Lesson: a
timeout inside a sequential multi-problem battery isn't good evidence of
a real budget shortfall for that one problem -- rerun it isolated before
concluding "needs more budget."

## Current defaults (as of this entry)

```
SearchConfig::invarianceLookahead   = 3     (bookkeeping only, never blocks progression)
SearchConfig::diversityRate         = 0.0   (confirmed: raising it doesn't help, can hurt)
GenConfig::loopAccumulatorBias      = 0.4   (confirmed peak, 160-run sweep)
GenConfig::indexIncrementBias       = 0.0   (opt-in; mixed A/B evidence, not a clean win alone)
GenConfig::bareSwapPenalty          = 0.0   (opt-in; same as above, be careful not to hurt sorting)
ExprKind now includes native Le/Ge/Ne  (base grammar; see section 9 -- broke mod's wall on 1/8 seeds)
```

## Open threads, roughly in the order they were flagged as next steps

1. Sorting stress test at a properly matched budget (loop-bias=0.4-ish,
   600-900s, 5 seeds) -- measure max confirmed N / max exact N / held-out
   / time, not "how many problems solved." If several seeds hit N=4
   confirmed and stop, the interesting question becomes *why* N=4->N=5
   specifically breaks, not "give it more time."
2. Composite/template-shaped priors (per finding #7) instead of
   per-statement probability nudges, if the sorting stress test also
   plateaus.
3. Contextual swap analysis before touching swap priors further (swap
   inside a loop vs. outside, after a comparison vs. not, involving the
   index vs. the accumulator) -- sorting may need swap to be *common* in
   exactly the contexts min/max/sum don't need it at all, so a naive
   "swap=bad" prior would be actively harmful there.
