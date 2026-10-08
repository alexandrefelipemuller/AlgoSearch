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

## 10. Four new curriculum problems: count, average, reverse, power

Added to broaden coverage past the original nine, each targeting one
specific compositional primitive not otherwise isolated:

- **`count`** (`src/problems/CountProblem.{hpp,cpp}`): counts array
  elements strictly greater than a threshold (query folded into the array
  like `ContainsProblem`: `input[0]` = threshold, `input[1..]` = haystack).
  Adds a counter driven by a condition inside the loop -- distinct from a
  running min/max/sum (pure aggregation) or a single membership check.
- **`average`** (`AverageProblem`): integer average of a non-negative
  array, `floor(sum/n)`. Needs a sum accumulator (like `SumArrayProblem`)
  *plus* a division step -- Mini-C has no divide primitive, so a
  generalizing solution needs division-via-repeated-subtraction on top of
  the sum, same idea as `ModProblem`'s curriculum role for `gcd`. Domain
  restricted to non-negative values on purpose (same reasoning as
  `ModProblem`/`GcdProblem`'s documented domain restrictions): truncating
  division via repeated subtraction is only unambiguous without negative
  operands, and teaching sign-handling is out of scope here.
- **`reverse`** (`ReverseProblem`): reverses an array (full-array output,
  same shape as `SortingProblem`, but the target permutation is fixed and
  known ahead of time rather than depending on value comparisons). Adds a
  two-independently-moving-index loop condition instead of the single
  counter every other array problem here uses.
- **`power`** (`PowerProblem`): `base^exponent` for non-negative
  integers, same magnitude-problem convention as `FactorialProblem`/
  `GcdProblem` (`input = {base, exponent}`). Needs multiplication via
  repeated addition inside a nested loop over the exponent -- structurally
  the same shape `FactorialProblem` needs, hence the same `numVars >= 5`
  default.

All four wired into `main.cpp`'s `makeProblem()`/usage string/error
message, with unit tests for the three array-shaped ones in
`array_problems_tests.cpp` (isSolution, partialScore, and an
instance-generation correctness sweep each -- `power`/`average` get no
dedicated unit tests, matching the existing convention that the other
magnitude problems (`gcd`/`mod`/`is_prime`/`fibonacci`/`factorial`) are
validated via CLI/benchmark runs, not unit tests). `make test`: 58/58 (49
original + 9 new), build clean under `-Wall -Wextra`.

**Quick difficulty read at a modest budget** (`--num-vars 5 --beam-width
500 --candidates 1500 --max-program-size 150`, no dedicated curriculum
work done yet): `reverse` solves and independently generalizes
immediately -- found a double-nested-loop swap solution (not the
minimal two-pointer-from-both-ends shape the header comment describes as
"natural," but a valid, verified `O(n^2)` reverse all the same:
```
v4 = 0;
while (v4 < v0) {
  v2 = v4; v1 = v4 + 1;
  while (v1 < v0) { swap(arr[v1], arr[v2]); v1 = v1 + 1; }
  swap(arr[v4], arr[v2]);
  v4 = v4 + 1;
}
```
`count`, `average`, and `power` all plateau below 100% at the first level
or two within this budget (partial scores climb sensibly -- e.g. 23/30,
6/8, 12/16, so the gradient exists and the search isn't stuck at zero --
it just hasn't composed the missing primitive yet). Consistent with this
project's established pattern for `gcd`/`mod`/`is_prime`: a new
compositional primitive (division, or a counter-under-a-condition, or
nested multiplication-via-addition) usually needs dedicated curriculum
work, not just "run it," before it's found unaided.

**Follow-up, same day: did the curriculum work.** First ruled out the
"factorial CPU-contention" false alarm from section 10's own earlier
lesson -- reran all three isolated at 300s each; all three ran their full
750 rounds (150 sizes x 5 seeds) to completion, no timeout, and still
plateaued (`count` 25/30, `average` 6/8, `power` 12/16). Genuinely stuck,
not budget-starved.

Then did the same code-level diagnosis that found `gcd`'s missing
`Sub(Var,Var)` weighting and missing `!=`, this time by reading
`ProgramGenerator.cpp`'s existing skeleton library instead of the
`ALGOSEARCH_DEBUG_LANDSCAPE` diagnostic (the grammar gap here was visible
directly in the code, not just empirically):

- **`count`**: `AccumKind` only had `{Sum, Min, Max, Membership, Swap}` --
  no accumulator increments a counter GATED BY A CONDITION. `Membership`
  sets a flag (`acc = 1`); nothing does `acc = acc + 1` under an `if`.
- **`average`**: `Sum` exists, but no skeleton anywhere computes a
  QUOTIENT via repeated subtraction -- `NestedDivisor` only checks
  divisibility (primality), `GcdEuclid` computes a remainder-driven
  alternation, neither tracks "how many times did that subtraction
  succeed."
- **`power`**: the only multiply-via-repeated-addition skeleton
  (`genFactorialSkeletonBlock`) is hardcoded to factorial's exact
  arrangement -- an INCREASING multiplicand (`i`, 2..n) bounded by
  `arr[0]+1`. Power needs the opposite: a CONSTANT multiplicand (`base`,
  captured once from `arr[0]`) bounded by `arr[1]` (the exponent). Not a
  generalization of Factorial's skeleton, a different shape entirely.

Fixed each, following the exact same "one dedicated skeleton per shape"
precedent already used for `GcdEuclid`/`Fibonacci`/`Factorial`/
`NestedDivisor`/`SelectionSort` (`src/search/ProgramGenerator.cpp`):
- Added `AccumKind::CountIf` (reuses `Membership`'s captured-target-plus-
  vi-starts-at-1 convention, but increments a counter under `arr[vi] >
  threshold` instead of setting a flag under `==`) -- a genuinely generic
  fifth accumulator shape, not `count`-specific.
- Added `genAverageSkeletonBlock`: sum-the-array (identical to the `Sum`
  AccumKind's loop) then divide by var 0 (the interpreter's auto-seeded
  array-length scalar) via repeated subtraction, using the native `Ge`
  operator (section 9) for a plain `while (remainder >= n)` with no
  compound condition needed. Offered as ONE atomic two-loop block rather
  than hoping two independently-drawn generic skeletons compose (each
  skeleton call picks its own fresh scratch variables, so a separately-
  drawn "divide" template would need to land on the exact same variable
  the `Sum` loop happened to accumulate into purely by chance -- the same
  composability wall documented in section 7) -- same precedent as the
  `SelectionSort` library-learning experiment.
- Added `genPowerSkeletonBlock`: captures `base = arr[0]` once, then a
  nested loop structurally like Factorial's but multiplying by the
  captured constant instead of an increasing counter, bounded by
  `arr[1]`.

Result: **all three solve immediately** at the same modest budget that
previously plateaued them (`--num-vars 6`, since `power`'s skeleton needs
5 scratch scalars + var 0), independently re-verified via `--benchmark`,
0 rejected:
```
# count (CountIf)
v1 = arr[0]; v4 = 0; v5 = 1;
while (v5 < v0) { if (arr[v5] > v1) { v4 = v4 + 1; } v5 = v5 + 1; }
arr[0] = v4;

# average (sum then divide)
v4 = 0; v1 = 0;
while (v1 < v0) { v4 = v4 + arr[v1]; v1 = v1 + 1; }
v5 = v4; v3 = 0;
while (v5 >= v0) { v5 = v5 - v0; v3 = v3 + 1; }
arr[0] = v3;

# power (constant-multiplicand nested multiply)
v1 = arr[0]; v4 = 1; v5 = 0;
while (v5 < arr[1]) {
  v2 = 0; v3 = 0;
  while (v3 < v1) { v2 = v2 + v4; v3 = v3 + 1; }
  v4 = v2; v5 = v5 + 1;
}
arr[0] = v4;
```
No regression: `make test` 58/58; all 11 pre-existing problems (`sorting`
through `fibonacci`, plus `reverse`) still save solutions at a quick
smoke-test budget after this change, build clean under `-Wall -Wextra`.

## 11. Consolidated 8 skeletons down to 6

The skeleton library had grown to 8 top-level options (`Counting`,
`GcdEuclid`, `Fibonacci`, `NestedDivisor`, `Factorial`, `SelectionSort`,
`Power`, `Average`), each an equally-likely draw whenever
`loopAccumulatorBias` triggers -- meaning more skeletons dilutes the
sampling probability of any one of them. Two pairs were actually the same
loop SHAPE with one parameter varying, not genuinely different
algorithms, so merged them the same way `Counting` already handles its
five accumulator variants (one draw, `AccumKind`-style internal choice):

- **`Recurrence`** = `GcdEuclid` + `Fibonacci`: both "seed two scalars
  from the array, update them with Add/Sub every iteration of a single
  loop, write back." `RecurrenceKind` picks which body/condition
  (subtractive alternation gated on `!=`, or additive Fibonacci gated on
  a counter) -- `FibonacciAdd` only offered when `numVars >= 4` for its
  extra counter scalar, same gate it always had.
- **`RepeatedMultiply`** = `Factorial` + `Power`: both "multiply via a
  nested repeated-addition loop," differing only in whether the
  multiplicand is an INCREASING factor (`Factorial`) or a CONSTANT
  captured once (`Power`, section 10). `RepeatedMultiplyKind::
  ConstantFactor` only offered when `numVars >= 6` (needs the extra
  captured-base scratch scalar) -- below that it always builds
  `IncreasingFactor`, so `Factorial`'s original `numVars >= 5`
  availability is preserved exactly, not narrowed by folding `Power` in
  alongside it.

`NestedDivisor`, `SelectionSort`, `Average`, and `Counting` stayed
separate on purpose: none of them share a loop shape closely enough with
anything else here for a merge to be more than an artificial wrapper --
`NestedDivisor`'s body is arithmetic (repeated subtraction testing
divisibility), `SelectionSort`'s is array-element comparison-and-swap,
`Average` is two SEQUENTIAL single loops (not one, and not nested) rather
than one shared shape, and `Counting` already covers its own six-way
variance under one draw.

Bonus fix while touching `GcdEuclid`'s merged code: it was still building
`!=` as the pre-section-9 compound `(a<b)+(a>b)`, predating native `Ne`.
Now uses `Ne` directly.

No regression: `make test` 58/58; all 14 problems (the original ten plus
`count`/`average`/`power`/`reverse`) still solve every level at a quick
smoke-test budget after the consolidation, build clean under
`-Wall -Wextra`.

## 12. Cross-skeleton study: does any problem converge on someone else's skeleton?

Direct follow-up to consolidating skeletons (section 11): with 6 shapes
now (`counting`, `recurrence`, `nesteddivisor`, `selectionsort`,
`repeatedmultiply`, `average`), does any problem's search actually
converge using a skeleton that wasn't built for it? Added
`ALGOSEARCH_FORCE_SKELETON` (env var, not a CLI flag -- deliberately, see
the comment in `genStmt`: a per-problem CLI knob was already rejected
once for revealing the answer in the usage string, but a single override
applied uniformly across an experiment doesn't reintroduce that) to
restrict every skeleton draw to exactly one named shape, then ran 9
problems x 6 forced skeletons x 3 seeds (`scripts/run_cross_skeleton_study.sh`,
data in `data/cross_skeleton_study_20260916_2017/`), reading the max
level each combination actually reached (careful to match only standalone
`N=<k>` level-header lines -- an earlier pass double-counted numbers that
appeared inside the UNCONFIRMED warning text, like "breaks before N=7",
as if they were real progress).

**No full cross-convergence anywhere.** `gcd`/`fibonacci` only reach
N=8 with `recurrence`; `is_prime` only with `nesteddivisor`;
`factorial`/`power` only with `repeatedmultiply`; `average`/`count` only
with `average`/`counting` respectively. Every other forced skeleton for
these problems can't even clear the trivial N=1 (magnitude problems) or
N=1 (`average`/`count`, whose own real generalization needs their
specific accumulator/division piece). `mod` is the extreme case: it
never clears N=1 with ANY single skeleton forced, including every one
that exists -- confirming section 9's finding that its (rare, 1/8-seed)
solution needs the free, unrestricted composition pool, not a dedicated
shape at all.

**One genuine partial cross-convergence, though:** `sorting` reaches
N=4 when forced onto `counting` -- a skeleton built for min/max/sum/
contains/count, nothing to do with sorting -- while every OTHER foreign
skeleton (`recurrence`, `nesteddivisor`, `repeatedmultiply`, `average`)
gets stuck at N=1-2, same as the magnitude problems' foreign-skeleton
failures. The reason is structural, not coincidental: `counting`'s `Swap`
`AccumKind` variant is a single adjacent compare-and-swap pass
(`if (arr[vi] > arr[vi+1]) swap(...)`), and the discovered program wraps
that single pass in an OUTER while-loop that keeps re-running it --
literally bubble sort, composed from a piece that was never designed
with sorting in mind, just reused because a repeated bubble pass is,
mechanically, the exact same "counting loop wrapping a per-element
check" shape every other `AccumKind` already uses. It's not enough to
fully generalize (still `UNCONFIRMED` past N=4, unlike `selectionsort`'s
clean N=8), but it's a real signal that shared PIECES (the adjacent-swap
idiom) can cross skeleton boundaries even when the enclosing algorithm
can't.

Practical upshot: the skeleton library is about as consolidated as it
can honestly be -- section 11's two merges (`Recurrence`,
`RepeatedMultiply`) were real duplicate shapes; the remaining 6 are each
load-bearing and non-interchangeable for their own problem, confirmed
empirically rather than assumed. The one soft spot (`counting`'s `Swap`
variant partially serving `sorting`) is closer to an argument for
factoring "the adjacent-swap idiom" out as its own even smaller
building block than for merging two whole skeletons.

## 13. Chasing better asymptotic complexity: gcd O(log n), is_prime O(sqrt n) -- both hit the same wall

User question: can `gcd` reach real O(log n) (the textbook mod-based
Euclidean algorithm, not the current O(n)-worst-case subtractive one),
and can `is_prime` reach O(sqrt n) (bounding trial division by sqrt(x)
instead of x)? Implemented both as new skeleton variants, then measured
the actual complexity directly against hand-built reference programs
(bypassing the search entirely, which always finds the smaller/simpler
program first regardless of asymptotic quality -- see below) rather than
trusting the `--benchmark` curve fit on whatever the search happened to
discover.

**`gcd`: added `RecurrenceKind::GcdMod`** (`(a,b) = (b, a mod b)`,
`a mod b` computed via its own repeated-subtraction inner loop, gated on
`canNestedLoop` since it's a 2-loop shape) to the `Recurrence` skeleton
alongside the existing `GcdSubtract`. The search itself never picks it
over `GcdSubtract` in practice -- across 10 `--benchmark` seeds forcing
`ALGOSEARCH_FORCE_SKELETON=recurrence`, all 7 distinct solutions found
were the smaller subtractive program (`GcdSubtract` fits in fewer AST
nodes, so the size-by-size growth sweep finds it first every time; the
search optimizes for correctness+size, never for the discovered
program's own runtime). To measure `GcdMod`'s actual complexity
independent of whether the search ever finds it, hand-built both
programs and timed them directly (bypassing Problem/BeamSearch
entirely) across three instance families:

| family | GcdSubtract exponent | GcdMod exponent |
|---|---|---|
| `gcd(n, 1)` (worst case for both) | 0.96 | 1.00 |
| consecutive Fibonacci pairs (classic Euclidean-iteration-count worst case) | 0.08 | 0.08 |
| typical balanced pairs (b ~ half of a) | 0.14 | 0.14 |

**Same exponent in every regime tested -- `GcdMod` buys nothing
asymptotically.** The reason is exactly the honest caveat flagged when
it was added: computing `a mod b` via repeated subtraction costs
O(a/b) -- so whenever one operand is much smaller than the other
(`gcd(n, 1)`: b=1 forces n subtractions in the very first mod step),
`GcdMod` inherits the identical O(n) blowup `GcdSubtract` already has.
Interesting side-finding: `GcdSubtract` is ALREADY near-logarithmic on
Fibonacci pairs and typical balanced inputs (exponent ~0.08-0.14, not
1.0) -- its O(n) worst case is a narrow pathological edge (extreme
operand-ratio inputs), not the general case. `GcdMod` doesn't improve on
any of the three regimes; it just implements the same complexity
differently. **Conclusion: no, not reachable with Mini-C's current
primitives** -- true O(log n) needs O(1)-ish division, which needs a
native multiply/divide/modulo instruction Mini-C doesn't have. Kept
`GcdMod` in the grammar anyway (it's a real, correct algorithm, verified
via `--benchmark` at 10/10 held-out), just not the complexity win hoped
for.

**`is_prime`: bounded `NestedDivisor`'s outer loop by `d*d <= x`**
instead of `d < x`, tracking `d`'s square incrementally via pure Add
(`sq += 2d+1` each step, no multiply needed -- see the code comment).
Gated on `numVars >= 5` (one tier up from the original). Hand-built both
naive and sqrt-bound programs and timed large primes (worst case: no
divisor ever found, loop runs to its bound) directly:

| | naive (`d < x`) | sqrt-bound (`d*d <= x`) |
|---|---|---|
| exponent | 1.09 | 1.07 |
| time at x=256019 | 117.9ms/1000 calls | 49.8ms/1000 calls |

**Same exponent again, but a real ~2.4x constant-factor speedup.** The
reason is the same shape as `gcd`'s: the OUTER loop bound did shrink
from O(x) to O(sqrt(x)) candidates as intended, but each candidate's
divisibility check (repeated subtraction, `r -= d` until `r <= d`)
costs O(x/d), and `sum(x/d)` for `d` from 2 to `N` is `~x*ln(N)` --
whether `N` is `x` (naive) or `sqrt(x)` (bounded), the total is
`O(x log x)` either way, only differing by the constant `ln(sqrt(x)) =
0.5 ln(x)` -- which is exactly the ~2x measured. **Conclusion: same
answer as gcd, for the same underlying reason** -- cutting the number of
candidates checked doesn't help when each candidate's own check cost
(not the candidate count) is the actual bottleneck, and that per-
candidate cost is itself linear-in-value because it's built from
repeated subtraction. A genuine O(sqrt(n)) total would need O(1)-ish
divisibility testing, i.e. real division.

**The general lesson, stated once for both:** Mini-C's Add/Sub-only
arithmetic (no native multiply/divide/modulo) is a hard ceiling on which
VALUE-dependent complexity classes are reachable, independent of how
clever the loop-bound skeleton is. Smarter skeletons can still deliver
real constant-factor wins (is_prime's ~2.4x) or correctness/robustness
without an asymptotic win (GcdMod), and those are worth having, but
"give it a better skeleton" cannot substitute for a missing arithmetic
primitive the way it substituted for a missing control-flow shape
(sections 9-10's native comparisons, `CountIf`, division-via-
subtraction for `average`, etc.) -- this is a genuinely different kind of
limitation than every other wall documented in this file so far, one in
the LANGUAGE's expressiveness rather than the search's exploration or
the grammar's bias.

No regression: `make test` 58/58; all 14 problems still solve every
level at a quick smoke-test budget after both changes.

## 14. Sorting reaches O(n log n) -- heapsort, gated behind maxLoops=4

Direct follow-up to section 13's honest "no" on gcd/is_prime: those hit
a LANGUAGE ceiling (no multiply/divide). Sorting's O(n log n) ceiling is
different -- an ARCHITECTURE one (no second array rules out merge sort,
no recursion/stack rules out quicksort) -- and heapsort is fully
in-place and iterative, so it doesn't need either. Implemented it as
`genHeapSortSkeletonBlock`.

Two adaptations for what the grammar can/can't express, not just
translating textbook heapsort directly:
1. No multiply/divide, so the classic build-heap start index (`n/2 - 1`)
   is unreachable. Starts from `n-1` instead (every index, not just
   internal nodes) -- sift-down from a leaf is an immediate no-op
   (finds no valid child), so this still builds a correct max-heap, just
   with some redundant no-op work that doesn't change the overall
   O(n log n) bound (the extract phase already dominates it).
2. No `break` and no `&&`/`||` (the additive `!=` trick from section 9
   doesn't generalize to AND). Sift-down's usual "loop until no swap
   needed" became a `while (done == 0) { ...; if (no swap needed)
   done = 1; }` flag loop, and the two-condition check ("a valid right
   child exists AND it's bigger") is nested `if`s instead of a boolean
   expression -- control flow gives the same short-circuit-AND behavior
   for free.

Needs 4 while-loops (build-heap wrapping sift-down, extract-max wrapping
its own sift-down) -- raised `GenConfig::maxLoops` from 2 to 4
specifically for this (purely additive: existing 1-2-loop skeletons
never have to use the extra budget). Added as a 7th top-level skeleton,
gated on a new `canQuadLoop` check and `numVars >= 5`.

**Verified correct and genuinely faster, but the free search never
finds it on its own.** Forcing it (`ALGOSEARCH_FORCE_SKELETON=heapsort`)
at a generous budget (beam=1500, candidates=5000, size=400, 10
`--benchmark` seeds, max-n=12): 9/10 seeds genuinely generalizing, 0
rejected, measured exponent **1.13-1.26** across seeds -- a real,
consistent, measurably different complexity signature from
`selectionsort`'s **1.90-2.00** in the identical setup (n log n isn't a
pure power law, so a log-log fit over a bounded range shows up as
"a bit above 1," not exactly 1 -- this is the expected signature, not
noise). Running the SAME generous budget with no skeleton forced (free
choice among all 7) found `selectionsort`/bubble-shaped O(n²) solutions
in all 10 seeds, never heapsort -- unsurprising given heapsort's AST is
~131 nodes against selection sort's ~37; the search's own size-by-size
growth sweep finds the smaller structure first every time, same
dynamic as section 13's `GcdMod` never winning over `GcdSubtract`.

**Real cost found and fixed before calling this done:** giving
`HeapSort` an equal 1-in-7 uniform draw (matching every other
skeleton's weight) measurably slowed down EVERY problem's search, not
just sorting's -- `reverse` at a modest budget went from ~15s to not
converging within 3+ minutes once `HeapSort` joined the uniform draw,
because a large, often structurally-irrelevant 131-node candidate
eating a 1-in-7 share of every biased statement pollutes the candidate
pool at every size step, for every problem, not just the one it could
help. Fixed by switching the skeleton draw from uniform to weighted
(`HeapSort` at 1/3 the weight of every other skeleton) -- `reverse`
back to 3.2s, `sorting` with `heapsort` forced still resolves in
0.07s, all 14 problems still solve every level at a quick smoke-test
budget afterward.

**Answers the original three-part question concretely:** `is_prime`
O(sqrt n) and `gcd` O(log n) are not reachable with Mini-C's current
arithmetic primitives (section 13) -- a language ceiling. `sorting`
O(n log n) IS reachable and now implemented and verified (heapsort) --
an architecture problem, solvable by extending the grammar/loop budget
rather than needing new primitives -- but reaching it currently requires
the research-only `ALGOSEARCH_FORCE_SKELETON` override, not the free
search, which reliably prefers the smaller O(n^2) shape. Whether that's
"good enough" depends on the goal: if it's finding SOME O(n log n)
sorting via unassisted search, that remains open (same avoid-the-biggest-
AST-first dynamic biasing away from it that every "smaller shape wins
the race" finding in this file has shown); if it's demonstrating the
grammar/architecture CAN express O(n log n) sorting at all, that's now
answered yes.

No regression: `make test` 58/58; all 14 problems solve every level at
a quick smoke-test budget, build clean under `-Wall -Wextra`.

## 15. 3SUM: free search fails cold, a dedicated 3-index skeleton fixes it outright

Tried a new problem for variety (`ThreeSumProblem`, `src/problems/
ThreeSumProblem.{hpp,cpp}`, registered as `--problem three_sum`): do
three DISTINCT indices i, j, k exist in the array with
`arr[i]+arr[j]+arr[k] == 0`? Output convention matches the other
decision problems (`contains`-style: `output[0]` is 1/0). Needs
negative values to be answerable at all, so instances are crafted
directly rather than reused from `ArrayInstances`' mostly-non-negative
base set: for each base/structural array, one "no" variant (perturbed
until genuinely triple-free, checked by the same O(n^3) brute force
used as the ground-truth oracle) and three "yes" variants with a
zero-sum triple force-written at three different index patterns
(front/middle/back) -- the same "don't let one fixed position become a
shortcut" fix `ContainsProblem::addBothCases` already documents.

**Free search, no dedicated skeleton: genuinely couldn't solve it, not
even at N=3.** First pass (beam=500, candidates=1500, num-vars=6,
~40s) stalled at 86-99% on N=3-4 and never closed. Assuming it just
needed more budget, reran at 4x the beam/candidates with all 8 threads
for ~65 minutes (`data/` run, `/tmp/threesum_run1.log`): it DID
eventually hit 100% training score at N=3 and N=4, but both were
`UNCONFIRMED` -- broke before N=6 with the unmutated program, and the
N=4 "solution" was visibly not implementing anything triple-related (an
accumulate-then-count mess that only coincided with all 128 of that
level's instances). N=5 never even closed (stuck at 510/512) before the
run ended. **Ten times the budget, 100x the wall-clock, zero genuinely
generalizing programs found.** Unlike every other magnitude/array
problem in this project (`contains` included, which resolves in
<1s), the free grammar-composition search has no path INTO the
"compare three related elements" shape -- none of the 6 existing named
skeletons relate more than two array elements or nest more than 2
loops for a comparison (the closest, `nesteddivisor`, nests 2 loops but
for divisibility, not element comparison), and the plain unstructured
statement grammar essentially never stumbles into a correct triple-
nested-loop-plus-flag program by chance at these sizes.

**Added a 7th skeleton, `genThreeSumSkeletonBlock`** (`src/search/
ProgramGenerator.cpp`): a genuine triple-nested `while` loop (i, then
j=i+1, then k=j+1, each bounded by `v0`) with `found=1` set inside the
innermost `if (arr[i]+arr[j]+arr[k] == 0)`, writing `arr[0] = found` at
the end -- literally the textbook O(n^3) reference algorithm. Needs a
new `canTripleLoop` gate (`maxLoops - loopsUsed >= 3`, one tier below
`HeapSort`'s existing `canQuadLoop`) and 4 scratch scalars (i, j, k,
found), so gated on `numVars >= 5` same as `nesteddivisor`/`average`.
Weighted at 3 (same as every skeleton except `HeapSort`) in the draw --
its AST is small (~15 nodes, nothing like `HeapSort`'s ~131), so unlike
that finding there's no a-priori reason to expect it to pollute other
problems' candidate pools.

**Fixed it outright, both forced and free.** `ALGOSEARCH_FORCE_SKELETON=
threesum` at a modest budget (beam=500, candidates=1500) finds the
EXACT reference triple-loop program, confirmed through N=8, held-out
52/52, in 0.53s. More importantly, the FREE search (no override, all 7
skeletons competing) now also finds a genuinely generalizing solution
completely unassisted: `first_level 3`, `confirmed_levels 6`, held-out
52/52, `Search finished in 0.127s` -- from "10x budget / 65 minutes /
zero generalizing solutions" to "under a second, unforced" by adding
exactly the one shape the free composition search had no other way to
reach.

**No regression checked directly** (this project's heapsort finding
established that a wrongly-weighted new skeleton CAN slow down every
OTHER problem, so this wasn't assumed): `make test` still 58/58. A
smoke-test sweep across the other 14 problems post-change showed
`reverse`/`mod`/`power` all slower than a quick glance expected (`reverse`
over 40s, `mod`/`power` 20+s) -- looked like the same HeapSort-shaped
regression at first, but a `git stash`-and-rerun of the exact same sweep
on the PRE-change binary reproduced the identical slowness (`reverse`
also >40s, `mod`/`power` also ~20s), which rules out `ThreeSum` as the
cause -- this specific combination of flags (`num-vars=5`,
`candidates=1500`, `seed=1`, `--loop-bias 0.4`) is just slow for those
three problems already, unrelated to this change. Worth remembering:
check the baseline before attributing a slow run to the latest diff,
even when the shape of the slowdown matches a documented past bug.

## 16. Where this sits relative to the academic/industrial state of the art

Literature check (web search, September 2026) after the 3SUM episode
made the project's core dynamic unusually legible: is this beam search
fast/capable relative to what's actually out there for "discover an
algorithm by search"?

**Speed: competitive, but for a reason worth being honest about.**
`PSB2` (the standard general-program-synthesis benchmark suite,
includes a GCD task) recommends capping each PushGP run at **60
million** program executions, and even combining PushGP with GPT-4o
only closes 23/25 of its tasks -- classic genetic programming alone
does noticeably worse per-task. This project's runs solve most problems
in well under a second to low tens of seconds, generating thousands to
low hundreds of thousands of candidates -- several orders of magnitude
cheaper in raw evaluation count. Enumerative/deductive synthesis
(SyGuS-style, counterexample-guided) lands in the same
millisecond-to-second speed class, but for a narrower problem class
(loop-free expressions guided by a handful of I/O examples); synthesizing
*loops* from scratch, which is what every problem here actually needs,
is a much harder and less mature corner of that literature.

**The speed isn't free, though -- it's bought by the skeleton library,
and section 15 just demonstrated that in the most direct way possible.**
Every named skeleton (`counting`, `selectionsort`, `recurrence`,
`threesum`, ...) is close to the target algorithm's shape already
encoded as a template; the free search's job is closer to "wire the
right variables into a pre-built structure" than "invent the
structure." 3SUM made this undeniable: the untouched grammar failed for
65 minutes at 10x budget, and flipped to sub-second unassisted success
the moment a human wrote the triple-nested-loop template in by hand.
That's the same "language ceiling" pattern section 13 already found for
`gcd`/`is_prime`'s better asymptotic variants, now confirmed a third
time on a completely different problem shape -- this project's actual
search algorithm (beam search + structural-invariance/held-out
verification) is solid and arguably more rigorous about not fooling
itself than a lot of toy GP demos, but the RESULTS ceiling is set by how
many algorithm shapes a human has pre-encoded as skeletons, not by the
search's own exploration power.

**Capability ceiling: far from the actual current frontier.** The real
state of the art for "discover an algorithm via search" as of 2025 is
no longer classical GP -- it's LLM-guided evolutionary search:
DeepMind's FunSearch (2023) and its successor AlphaEvolve (2025), plus
2025 open-source descendants (OpenEvolve, ShinkaEvolve, CodeEvolve).
These evolve whole programs with an LLM supplying the mutation/
recombination prior instead of a human-written grammar, and have
produced results with **no pre-existing human template to fall back
on** -- new constructions for the cap set problem, new bin-packing
heuristics, and (via the sibling AlphaTensor line) matrix-multiplication
algorithms nobody had written down before. That is exactly the
capability this project's approach structurally cannot reach without a
different mutation operator: every "discovery" here traces back to a
human already knowing and encoding the target shape, which is the
opposite of what makes FunSearch/AlphaEvolve's results notable.

**Net position:** ahead of or level with classical GP baselines on
raw search efficiency for problems whose shape is already in the
skeleton library; nowhere near the LLM-guided-evolution frontier on
genuine open-ended discovery, and structurally can't get there by
tuning beam width or adding bias knobs -- would need either an LLM (or
similar) supplying a general code-mutation prior, or accepting that
growth here means one hand-written skeleton at a time, same as every
other finding in this file involving a language/architecture ceiling.

## 17. Breaking the language ceiling for real: native Mul/Div/Mod/bitwise, plus two new problem families

Direct follow-up to section 16's SWOT ("expand the vocabulary carefully"):
added native arithmetic (`Mul`, `Div`, `Mod`) and bitwise (`BitAnd`,
`BitOr`, `BitXor`, `Shl`, `Shr`) as `ExprKind` primitives (evaluated in
O(1) by the interpreter, overflow/div-by-zero/bad-shift all mapped to the
existing `RUNTIME_ERROR` bucket, same as an out-of-bounds array access --
nothing new crashes). **Foundational project decision, stated by
Alexandre and now binding: no LLM, no neural nets, no learned component
of any kind, ever** -- every extension in this section is pure grammar/
interpreter work, no exception considered.

**Gating, to protect the 15 existing problems:** the new ops are NOT
folded into `genValueExpr`'s core 4-way Add/Sub/ArrayAccess/Var choice
(that would dilute the carefully-tuned split the Sub(Var,Var) bug in
section 5 was about) -- instead an 8% independent substitution
probability, always-on for every problem (same "one universal trigger,
no problem-specific knob" principle as `loopAccumulatorBias`), same
precedent as native Le/Ge/Ne joining `genCondExpr`'s kinds array in
section 9.

### Breaking section 13's wall, confirmed both by hand and through the full search+verification pipeline

Section 13 found `gcd`/`is_prime` stuck at the same wall: a smarter
loop-bound skeleton shrinks the CANDIDATE count (O(log n) outer
iterations, O(sqrt n) divisor candidates) but each candidate's own check
was built from repeated subtraction, costing O(a/b) or O(x/d) -- so the
TOTAL work stayed O(n) / O(x log x) regardless of the loop-bound
cleverness. Native Mod/Mul removes that per-candidate cost entirely.

Added two new skeleton variants, kept alongside the originals (not
replacing them) so the A/B comparison stays measurable:
`RecurrenceKind::GcdModNative` (`(a,b)=(b, a%b)`, one native `%` instead
of a repeated-subtraction inner loop -- collapses from 2 loops to 1) and
`genNestedDivisorNativeLoopBlock` (`d*d<=x` bound via native `*`, `x%d==0`
divisibility via native `%` -- collapses from 4 scratch scalars to 2, no
inner loop at all, one tier looser on `numVars` than the original).

Hand-built-and-timed (bypassing search entirely, same methodology as
section 13's table):

| test | old (repeated subtraction) | new (native) |
|---|---|---|
| `gcd(n,1)`, n: 4000->400000 | exponent 0.99 (genuinely O(n)) | exponent ~0.00 -- **O(1)**, 100x n gives the same runtime |
| `is_prime`, large primes, x: 100003->10000019 | exponent 1.08 | exponent ~0.44, **10544x faster** at the larger size |

Then re-verified through the REAL pipeline, not just the hand-built
programs -- forcing `ALGOSEARCH_FORCE_SKELETON=recurrence` for `gcd` and
running free (unforced, default `loop-bias`) for both: **both problems'
search converges onto the native variant in every seed tested (8/8 for
gcd forced, 6/6 free for gcd, 6/6 free for is_prime)**, and
`--benchmark`'s own independent curve fit reports it plainly: `gcd` **"O(1)
-- constant"**, `is_prime` **"O(sqrt n) -- square root", r2=1.00**. The
free search prefers the native variant not because it was told to, but
because it's simultaneously smaller (fewer scratch vars, fewer loops)
than the repeated-subtraction version AND asymptotically faster -- unlike
`GcdMod` in section 13, which the free search never picked over
`GcdSubtract` specifically because it was the same speed but bigger.
**Conclusion: section 13's ceiling is not a permanent architecture limit
-- it was a real, specific consequence of Mini-C having no O(1)
arithmetic primitives, and adding exactly those (not a smarter skeleton,
not more search budget) removes it outright, confirmed both by hand and
end-to-end.**

### Two new problem families, testing the SWOT's coverage gap directly

**`single_number` (bit manipulation, LeetCode's "Single Number"):** every
array value appears an even number of times except one; find it. Reused
existing infrastructure almost for free -- added `AccumKind::Xor` to
`genCountingLoopBlock` (identical shape to the existing Sum/Min/Max/
CountIf accumulator kinds, just `acc = acc ^ arr[vi]`), no new dedicated
skeleton needed. **Free search finds the exact canonical XOR-reduce
solution unassisted**: `first_level 1`, `confirmed_levels 8`, held-out
18/18, `Search finished in 0.041s`. The easiest new capability added in
this project's history, because the shape was already 90% built.

**`graph_reachability` (graph algorithm, is vertex V-1 reachable from
vertex 0?):** the harder of the two, deliberately, to test whether a
genuinely different algorithmic shape (relational, not aggregative)
could be added at all. No pointers/structs in Mini-C, so the graph is
folded into the one flat array by convention: `[V, V scratch "reached"
cells, V*V row-major adjacency matrix]` -- the row-major indexing
(`base + i*V + j`) is the FIRST thing in this project that only exists
because of this section's arithmetic extension; before native `Mul` this
problem was flatly unrepresentable, not just hard to search. Algorithm
is fixed-point relaxation (repeat V times: mark j reached whenever i is
reached and edge i->j exists) since Mini-C has no queue/stack for real
BFS/DFS -- V rounds always suffices since the longest simple path has
fewer than V edges. Verified correct in isolation first (hand-built the
exact skeleton AST, ran it directly against 8 hand-constructed graphs --
chains, broken chains, cycles, disconnected -- 8/8 matched a reference
BFS before ever touching the search), the same "prove the algorithm
before trusting the search" discipline sections 13/15 established.

**Found and fixed a real gap in the benchmark's verification
infrastructure along the way**, not a bug in the algorithm: 5 of 7
initial `--benchmark` rejections were the magnitude-shift stress check
(section on `kShiftOffsets` in `Benchmark.cpp`, added to catch a formula
that only coincidentally works across a bounded value range) adding
997/1997 to EVERY array value -- including `graph_reachability`'s vertex
COUNT and boolean edge FLAGS, which have no meaningful "shifted"
interpretation at all (a shifted vertex count no longer matches the
array's real length; a shifted edge flag is neither 0 nor 1). This is
the same domain-appropriateness issue `GcdProblem`'s existing
shift-offset comment already flagged (positive-only offsets, to respect
gcd's positive-integers domain) but for a shape the two fixed offsets
can't rescue no matter what they're set to. Fixed properly rather than
special-cased away: added `Problem::supportsMagnitudeShift()` (default
`true`, so all 17 other problems are unaffected), overridden `false` for
`GraphReachabilityProblem`. After that fix, both a forced-skeleton run
and a completely FREE (default `loop-bias=0.4`, no override) run each
found a genuinely generalizing solution that survived full independent
re-verification: `--benchmark` fits it as **O(n^1.5)** (r2=0.84 -- noisier
than the clean r2=1.00 fits elsewhere, consistent with a real O(V^3)
algorithm's step count not being a pure power law over this size range,
same caveat section 14 made for heapsort's n log n). **The free search
finds this unassisted, unlike HeapSort** (section 14, never found free
because smaller irrelevant skeletons out-compete its ~131-node AST for
every OTHER problem's candidate slots) and unlike 3SUM's first attempt
(section 15, needed 65 minutes before concluding it was unreachable) --
here, no cheaper skeleton can score anything on V>=3 instances at all,
so there's no wrong-but-smaller competitor for size-preference to get
stuck on.

**Regression discipline, same as every prior addition:** `make test`
58/58; a `git stash`-based A/B sweep across all 15 pre-existing problems
at matched parameters showed no new slowdowns (`mod`/`reverse`'s known
flakiness at these specific parameters reproduces identically on the
pre-change binary, confirmed before attributing it to this change --
same lesson section 15 already learned the hard way).

**Net effect on the section 16 SWOT:** two of the three biggest coverage
gaps identified there (bit manipulation: 0%, graph algorithms: 0%) now
have one genuine, verified representative each, and the single named
"Weakness" from that SWOT with a concrete fix available (the arithmetic
language ceiling) is now closed for the two cases it was measured
against. What's NOT yet touched: the graph representation only supports
one relation (adjacency-matrix reachability) on artificially small V
(the O(V^2) instance size makes anything beyond V~10-15 expensive to even
generate/score), and DP/2D-table problems, strings, and real data
structures remain fully out of reach (still need a second array/table
primitive Mini-C doesn't have at all -- a different, larger architecture
change than this section's, not attempted here).

## 18. TSP, and the "middle ground" question: can capability grow without a human writing a new skeleton?

Added `TspProblem` (exact Hamiltonian-cycle-cost, ground truth via
host-side Held-Karp bitmask DP). Confirmed free/generic search gets
zero traction at all -- multiple runs up to num-vars=80/candidates=20000/
30 CPU-minutes never got past V=2 at ~8/14, with no improvement from
more compute, unlike every problem in sections 13-17. Two dedicated
skeletons were hand-authored to fix this:

- `genTspBruteForceSkeletonBlock`: iterative (no recursion -- Mini-C has
  none) Heap's-algorithm permutation enumeration. Confirmed generalizing
  via real search (`confirmed_levels` 3+ across seeds, held-out 8/8).
- `genTspBranchAndBoundSkeletonBlock`: iterative DFS with an explicit
  choice-stack and cost-based pruning. **Found and fixed a real
  correctness bug** while building it: the backtrack step read
  `path[level]` (to know which vertex to un-mark in `used[]`) BEFORE
  decrementing `level`, instead of after -- reading the wrong stack
  frame's slot, sometimes stale/zero, which could incorrectly un-mark
  vertex 0 (permanently used) and silently drop valid tours. Found by
  hand-tracing a failing V=3 instance (expected cost 12, got -1),
  confirmed fixed across 7 seeds/V=1-8 with a standalone harness before
  trusting it in the real search. Measured **2.6x-8.6x speedup** over
  brute force at V=6-10 from pruning alone, growing with V as expected
  of genuine pruning (not a constant-factor artifact).

TspProblem's scratch region is 4*V cells (wider than the brute-force
skeleton alone needs, at 2*(V-1)) so both skeletons can share one
instance encoding -- the brute-force skeleton just leaves the unused
cells alone.

### The middle-ground question: four independent attempts to avoid hand-authoring branch-and-bound, all negative

Going from brute-force to branch-and-bound required a human writing a
second, structurally different algorithm by hand -- a real, felt
ceiling: does skeleton *vocabulary* only grow by a human writing another
one? Full writeup, methodology, and exact commands in
`data/skeleton_middle_ground_experiments.md`; summary here:

1. **Crossover with existing skeleton vocabulary** (new
   `ALGOSEARCH_DISABLE_SKELETON` env var, a companion to the existing
   `ALGOSEARCH_FORCE_SKELETON`, lets one named skeleton be excluded
   instead of restricting to exactly one): negative even at
   crossover-rate=0.6. Heap's-algorithm brute force has no partial-path
   state to prune ON in the first place -- it always commits to a full
   permutation via swaps before ever computing a cost -- so grafting a
   guarded-comparison idiom from elsewhere (SelectionSort/ThreeSum's
   `if (x < best)`) has nowhere to earn its keep as a real prune.
2. **"Scaffold with a hole"**: a `guardAsHole` parameter on
   `genTspBranchAndBoundSkeletonBlock` (registered as a separate
   `tspbbscaffold` skeleton name for isolation, since removed -- see
   below) replaces the hand-written `cost < best` with an ordinary
   generated condition. The DFS/choice-stack SHAPE turned out to be
   reliably learnable/keepable once handed over (held-out 8/8 across 7
   seeds) -- but the hole always converges to a vacuous tautology
   (`mBase >= chBase`, a constant relationship always true for any V;
   other seeds found similar constant-derived non-guards), never
   anything resembling `cost < best`. Never generalizes to genuine
   pruning; correctness is preserved but speed never improves.
3. **Speed-aware fitness signal**: added `SearchConfig::speedBonusWeight`
   (0.0 default) -- adds `weight * (1 - steps_used/stepLimit)` to
   `fitness` per SUCCESS instance, never touching `score`/promotion, to
   test whether the hole converges to tautologies simply because nothing
   rewards being faster. Still negative even in the exact regime
   (V=8-9) where brute force already loses score to timeouts, i.e.
   where a pruning candidate should have a real fitness edge: the hole
   just finds a DIFFERENT tautology, or (once) a candidate that's wrong
   on held-out (7/8) rather than genuinely faster-and-correct.
4. **Finer mutation granularity + missing language primitive**: found a
   real, generically useful gap while investigating why the hole never
   refines after its first draw -- `growProgram`'s only point mutation
   replaces an ENTIRE if/while (cond + body + else-body), so any
   mutation touching a guard also destroys the correct logic nested
   inside it, and is essentially always fatal to score. Added
   `GenConfig::exprMutationRate` (0.0 default): regenerates just one
   if/while's `cond` in place via a new `collectCondSites` helper,
   leaving body/else-body untouched. Also added `Continue`/`Break` as
   real `StmtKind`s (interpreter `Flow` signal, gated by
   `GenConfig::controlFlowBias`, 0.0 default) -- Mini-C's missing
   early-exit primitive, the same "add a primitive, not a skeleton"
   move section 17 made for arithmetic. Both still negative on the core
   question (never found `cost < best`; guards found were `v4 != v2`-
   shaped near-tautologies with self-cancelling arithmetic), though the
   mutation fix incidentally improved held-out robustness at N=8 (8/8
   vs. the prior run's broken 7/8) and is worth keeping as a
   general-purpose operator regardless.
5. **All four stacked together** (crossover + control-flow +
   speed-bonus + expr-mutation simultaneously, `tspbbscaffold` forced):
   still negative -- if anything, the combined mutation pressure pushed
   the guard toward an even MORE trivial constant (`(1|2) >= 2`, not
   even referencing a variable) rather than a more selective one. Zero
   `continue`/`break` adoption throughout every test in this series.

**Working conclusion, stated plainly:** for a capability this
qualitatively different from what the search already has priors for (an
enumeration algorithm whose early-exit condition depends on a running
best-so-far evaluated against a PARTIAL solution, not a complete one),
none of recombination, partial scaffolding, a speed signal, or finer
mutation granularity -- alone or combined -- got the free search to
rediscover it. The most likely mechanistic reason: `cost < best` sits in
an isolated point of expression-space with no correct-and-selective
neighbors nearby, so there's no gradient for local search to climb even
once every other obstacle (vocabulary, a place to put it, an incentive,
room to tweak without breaking correctness) is removed. **The practical
strategy going forward is the one already stated in section 16's SWOT
and reaffirmed here empirically: keep expanding skeleton coverage by
hand for as much of the known algorithm catalog as the project can
reach, rather than expecting genuinely new algorithmic shapes to emerge
from search-mechanism tuning alone.** This doesn't rule out a
diminishing role for automatic discovery within a shape once it exists
(constant-tuning, minor structural variants) -- section 12's
cross-skeleton generalization work is exactly that -- but a new SHAPE
still needs a human to write it once, the way both TSP skeletons were.

**Kept vs. removed:** `Continue`/`Break`, `speedBonusWeight`, and
`exprMutationRate` are generic, zero-cost-when-off infrastructure in the
same spirit as `indexIncrementBias`/`bareSwapPenalty`/`scoreBandRate`
before them, and stay for whatever the next capability gap turns out to
need. `ALGOSEARCH_DISABLE_SKELETON` stays alongside `ALGOSEARCH_FORCE_
SKELETON` as a research-only pair. `tspbbscaffold` (the diagnostic
`guardAsHole` skeleton variant) was removed after this series concluded
-- it has no value outside the experiment that used it, and left in the
default pool it would only ever cost future TSP searches a wasted
candidate slot.

Regression discipline: `make test` 58/58 and a clean ASan/UBSan build
after every change in this section.

## 19. DTW / Frechet-distance-shaped problem: a SETH-conjectured-optimal O(n^2), not just "best known"

Added `DtwProblem` (`dtw`): global-alignment Dynamic Time Warping between
two equal-length integer sequences A, B (`table[i][j] = |A[i-1]-B[j-1]| +
min(up, left, diag)`, boundary `table[0][0]=0`, else INF; answer
`table[n][n]`). Same motivation as the user's original ask (Frechet
distance / DTW, both O(n^2)): unlike every earlier "can we beat the
naive complexity" thread here (section 13's gcd/is_prime, which *did*
have a better algorithm to find), DTW/edit-distance/LCS/Frechet distance
are all proven SETH-hard for any O(n^{2-eps}) -- Bringmann & Kunnemann
2015 (Frechet), Backurs & Indyk 2015 (edit distance), Abboud/Backurs/
Vassilevska-Williams 2015 (LCS, and by the same technique DTW). So O(n^2)
here isn't a weak baseline waiting to be beaten, it's the conjectured
final answer -- a genuinely different kind of "ceiling" than gcd's, where
this project should NOT go looking for something asymptotically better.

Mini-C has no 2D arrays, so the DP table is flattened row-major into a
scratch region the program must initialize itself, same convention as
`GraphReachabilityProblem`/`ShortestPathProblem` (`supportsMagnitudeShift()
= false` for the same non-shiftable-metadata reason).

**Followed the project's standard two-phase protocol:**
1. **Free search first, no dedicated skeleton** (`ALGOSEARCH_DISABLE_
   SKELETON` not even needed -- none existed yet): stuck at 11/15 on
   N=1 within a 20000-step/4.4s budget, matching every other DP-shaped
   problem's cold-start behavior (graph_reachability, shortest_path,
   three_sum) before their skeletons existed.
2. **Researched the standard textbook algorithm, then wrote
   `genDtwSkeletonBlock`**: one boundary-init pass + one ordered double
   loop (i then j, both increasing) filling `table[i][j]` from
   already-computed up/left/diag neighbors -- no round/relaxation loop
   needed here (unlike Bellman-Ford-style shortest_path), since the DP's
   dependency order is already a straight line. 11 scratch scalars,
   `numVars >= 12` gate, `canTripleLoop` budget (boundary + outer +
   inner loop), same tier as ThreeSum/TSP.

**Result, matched-seed A/B:** with the skeleton in the pool, free search
(no env-var force) solves N=1..3 exhaustively in 0.04s total, and a
5-seed `--benchmark` run at N=4 gets 5/5 genuinely-generalizing solutions
(independently re-verified, not just SUCCESS-labeled), every one fit at
empirical exponent 1.84-1.88 with r^2=0.99 -- i.e. the search doesn't
just find *a* correct program, it converges on the true O(n^2) DP every
time, which is the right outcome for a problem where O(n^2) is
conjectured optimal rather than a target to beat. `ALGOSEARCH_FORCE_
SKELETON=dtw` alone (isolating the skeleton from the rest of the
generic grammar) reproduces the same 5/5 result, confirming the skeleton
itself is sufficient, same falsification-style check section 15 used
for ThreeSum.

Not yet folded into a long multi-seed sorting-style sweep; N=4 is as far
as tested so far. Held-out/adversarial instances (constant-shift,
ascending-vs-descending, all-equal-A) all passed at every level tried.

## 20. contains_ordered at a large budget + high diversity: still no binary search, matched A/B

Ran `contains` vs `contains_ordered` head-to-head with matched config
(`--max-n 10 --beam-width 4000 --candidates 4000 --max-program-size 80
--step-limit 30000 --num-vars 8 --diversity-rate 0.4`, `--benchmark
--benchmark-seeds 5`, same seed base) to test the hypothesis in
`ContainsOrderedProblem.hpp`'s own comment: would raising budget and
diversity let the same generic search spontaneously discover a
sub-linear (doubling/bisection) shape once the data (a sorted haystack)
actually rewards it, given `Div` is reachable (8% substitution chance in
`genValueExpr`, see section 17) so `mid = (lo+hi)/2`-style bisection is
at least expressible.

**Result: negative, clean 5/5 vs 4/4 match.** Both problems converged to
the exact same program shape in every seed -- a plain linear scan, and
`contains_ordered`'s version doesn't even add an early-exit once the
target is found/passed:
```
v1 = arr[0]; v3 = 0; v6 = 1;
while (v6 < v0) { if (arr[v6] == v1) { v3 = 1; } v6 = v6 + 1; }
arr[0] = v3;
```
Both benchmark at O(n), exponent ~0.96-1.07, r^2>=0.97-1.00 -- no
measurable exploitation of sortedness at all, let alone sub-linear
comparisons. Timing is near-identical between the two problems (contains
56-114us vs contains_ordered 56-62us at max N), consistent with the same
algorithm running on both.

This lines up with section 18's TSP conclusion (free search cannot
compose an unfamiliar *shape* -- there, "prune using a partial-solution
bound"; here, "stop early / bisect using a monotonicity precondition" --
from generic mutation/diversity alone) rather than contradicting it:
diversity and budget widen exploration of the SAME representational
neighborhood, they don't create a path to a qualitatively different
control-flow idiom (`while (lo<hi) { probe mid; halve the range }`) that
the grammar can express but essentially never stumbles into by chance
given how deep in expression-space `Div` sits at an 8%-per-node
substitution rate. No dedicated `containsordered`/binary-search skeleton
exists yet in `ProgramGenerator.cpp` -- adding one (mirroring
`genDtwSkeletonBlock`'s approach: hand-write the doubling/bisection
shape, verify it against the O(n) linear baseline via
`ALGOSEARCH_FORCE_SKELETON`) is the natural next step if sub-linear
`contains_ordered` is worth pursuing, same protocol as every other
missing-shape problem in this log.

Data: `data/contains_binarysearch_20260918/` (both run logs + solution
JSONs).

## 21. contains_ordered with ALL quota strategies + crossover + speed bonus: still no binary search

Follow-up to section 20's hypothesis discussion: does the beam's existing
niche-quota infrastructure (`shapeNicheRate`, `behaviorNicheRate`,
`scoreBandRate` -- BeamSearch.cpp:468-549, each reserving beam slots for
the best-scoring representative of a structural/behavioral/score niche,
protecting it from being crowded out by an already-100%-correct but
differently-shaped competitor) let a *raw, partially-correct* two-pointer
idiom survive long enough to mature into binary search, instead of being
discarded outright the instant a fully-correct linear scan appears?

Matched A/B, same budget class as section 20 but with every relevant
lever turned on at once: `--diversity-rate 0.2 --shape-niche-rate 0.2
--behavior-niche-rate 0.15 --score-band-rate 0.15 --crossover-rate 0.3
--speed-bonus-weight 0.05`, `--max-n 10 --beam-width 6000 --candidates
6000 --max-program-size 100 --step-limit 40000 --num-vars 8`,
`--benchmark --benchmark-seeds 5`, contains vs. contains_ordered.

**The quotas measurably did their job**: beam structural diversity at
full-solve roughly TRIPLED vs. section 20's plain-diversity-only run
(contains_ordered: ~1500-1660 distinct AST shapes / ~4200-4400
candidates here, vs. ~500-640 / 4000 before). So the mechanism the user
asked about -- "protect interesting raw candidates via quotas instead of
letting pure score dominate" -- is real and working as designed.

**Result: still negative, 4/4 and 5/5.** Identical program in every
seed of both problems, byte-for-byte the same shape as section 20 (still
no early-exit `break` even with the speed bonus on -- `controlFlowBias`,
which gates `if (cond) break/continue` generation, was left at its
default 0.0 in this run, so that idiom was never offered to the
generator regardless of what the fitness function would have rewarded
once it existed):
```
v2 = arr[0]; v1 = 0; v5 = 1;
while (v5 < v0) { if (arr[v5] == v2) { v1 = 1; } v5 = v5 + 1; }
arr[0] = v1;
```

**What this isolates:** tripling surviving structural diversity changed
nothing about the outcome. That rules out "the beam converges too fast
and prunes away the raw material before it can mature" as the bottleneck
-- there was 3x more raw material surviving, generation after generation,
and none of it was ever a two-pointer/bisection fragment, because
`scoreBandRate`'s guarantee is only "best-scoring member of this score
decile survives," not "best member *on the way to a specific coherent
algorithm* survives." A broken/partial bisection attempt (say, `lo`/`hi`
initialized but never correctly updated) scores in the same low bands as
every other unrelated wrong program, and there is nothing in a
score-decile or AST-shape-count signature that distinguishes "wrong but
structurally promising toward binary search" from "wrong and randomly
wrong" -- both look identical to every niche criterion currently
implemented, all of which are deliberately problem-agnostic (never
inspect *what a correct answer means*, only whether a candidate's score/
shape/behavior differs from what's already kept).

**Conclusion:** the bottleneck sections 18/20 already identified is
confirmed to sit at construction, not survival -- no realistic amount of
beam-diversity protection rescues a compound idiom (two converging
pointers + `Div`-based midpoint + branch-on-comparison to move one
pointer) that no single mutation/crossover event ever assembles enough
of to register as "closer" by any generic, problem-agnostic signal. A
genuinely new niche criterion built to detect "two scalars moving toward
each other under a shared loop condition" would be problem-specific
knowledge disguised as a general mechanism -- exactly the kind of
per-problem hint the project's `loopAccumulatorBias` design note argues
against. This closes out the "can generic search-mechanism tuning find
it" question for now: **the answer is no, matching TSP's precedent
exactly**, and the honest next step is a dedicated skeleton (built and
verified via `ALGOSEARCH_FORCE_SKELETON`, same protocol as `genDtwSkeletonBlock`),
not further tuning of `diversityRate`/`shapeNicheRate`/`scoreBandRate`/
`crossoverRate`/`speedBonusWeight`.

Data: `data/contains_quotas_20260918/` (flags, both run logs, solution JSONs).

## 22. Binary-search "ladder": does the tipping point arrive gradually, or all at once?

Direct follow-up to sections 20-21's open question: rather than jumping
straight to a full hand-written skeleton, built `genBinarySearchSkeletonBlock`
as a 4-rung ladder (`ALGOSEARCH_BSEARCH_STAGE=1..4`, registered as
`binarysearch` for `ALGOSEARCH_FORCE_SKELETON`) with strictly MORE hand-
written each rung, to find exactly where (if anywhere) free generation
can pick up and finish the rest on its own -- the direction-aware
two-pointer scan from section 20's comment (`lo=1; hi=v0; dir` via
first-vs-last comparison; loop until `lo<hi`; `arr[0]=found` at the end
is hand-written at every stage, since it's boilerplate, not the
algorithmic content under study):

1. Only the converging LOOP FRAME hand-written; the entire body
   (including whether to compute a midpoint at all) is free-generated.
2. Frame + midpoint (`mid = lo + (hi-lo)/2`, `v = arr[mid]`) also hand-
   written; the found-check and lo/hi-movement decision is free.
3. Frame + midpoint + the found-check/branch STRUCTURE hand-written;
   only the final "which way to move" comparison is a hole (the direct
   analog of TSP's `cost < best` hole from section 18).
4. Fully hand-written, no holes.

**Bug found and fixed before any real data**: stage 1 segfaulted
immediately -- `ALGOSEARCH_FORCE_SKELETON=binarysearch` pins EVERY
loop-bias skeleton draw to this skeleton, and a hole filled via
`genBlockStmts` can itself trigger a fresh loop-bias draw, which (being
forced) re-selects this same skeleton, which fills its own hole the same
way... unbounded recursion (confirmed via gdb: ~15,000 stacked
`genStmt`/`genBlockStmts`/`genBinarySearchSkeletonBlock` frames) until
the stack overflows. Fixed by generating every hole's content against a
LOCAL loop counter pre-set to `cfg.maxLoops` (never the real
`loopsUsed`), so `canLoop` reads false inside a hole and it can add
`If`/`Assign`/`Swap` freely but never a new `While` -- correct anyway,
since a hole here stands in for decision logic inside the loop this
skeleton already built, not a second loop. `make test` 58/58 after the
fix; re-ran the full ladder clean.

**Result, matched 5-seed `--benchmark` per rung, same budget throughout
(`--max-n 8 --beam-width 3000 --candidates 3000 --step-limit 20000
--num-vars 9 --loop-bias 1.0`, `ALGOSEARCH_FORCE_SKELETON=binarysearch`
so ordinary linear-scan noise can't contaminate what's being measured):

| stage | hand-written | result |
|---|---|---|
| 1 | frame only | 0/5 -- every seed stuck at N=2, 60/80, ~700s each |
| 2 | + midpoint | 0/5 -- identical plateau (60/80 at N=2), midpoint alone bought nothing |
| 3 | + branch structure, comparison is a hole | 1/5 solves, but degenerates to **O(n)**, not O(log n) |
| 4 | fully hand-written | 5/5, genuinely generalizing, exact algorithm |

Stage 3's one surviving solution is the same failure mode section 18
already found for TSP's `guardAsHole`: the hole converges to a
tautology (`v0 < 1`, always false for any real haystack) that makes an
entire branch permanently dead code, while the branch that's ALWAYS
taken degenerates the search into `hi = hi - 1` every iteration (plus an
unrelated `swap`) -- i.e. it satisfies correctness by shrinking the
range by a constant 1 each time instead of halving it, which is exactly
why it benchmarks as linear rather than logarithmic. **This is not a
smaller/weaker version of binary search -- it's a different, slower
algorithm that happens to still be correct**, discovered specifically
because it was easier to construct than genuine bisection.

**Conclusion: there is no gradual tipping point.** Free generation
can't take advantage of the frame alone (1), the frame+midpoint (2), or
even a fully-structured branch skeleton missing only one comparison (3)
-- at every rung short of writing the whole thing, it either stalls
completely or finds a degenerate correct-but-not-faster escape hatch.
Only handing over the ENTIRE algorithm (4) produces the real answer.
This sharpens sections 18/20/21's standing conclusion rather than
contradicting it: it's not just that new algorithmic shapes need a
skeleton -- it's that even generously scaffolded PARTIAL skeletons don't
reliably get finished correctly by free search either, because "still
correct, but via a cheaper degenerate shortcut" is always closer at hand
than the genuinely intended structure once only one piece is missing.

**Next question this opens, not yet run:** stage 4 is a real, complete,
validated skeleton -- the natural next step (same as every other
`gen*SkeletonBlock` in this file) is adding it to the default pool
un-forced and checking whether it ever gets SELECTED over the
already-known linear scan once both are available, given
`BeamSearch.cpp`'s score-then-size tie-break (section 20's mechanistic
finding: a same-scoring but structurally BIGGER program always loses
that tie-break) -- i.e. does `speedBonusWeight` need to be on for the
size-67 O(log n) skeleton to ever outrank the size-27 O(n) scan once
both are constructible, or does it win anyway once it reaches 100% first
in some run order. Not yet tested.

Data: `data/bsearch_ladder_20260918/` (per-stage logs + solution JSONs).

## 23. Section 22's open question, answered: the skeleton never gets a chance to compete, regardless of speedBonusWeight

Ran the two matched cases section 22 left open: `contains_ordered` with
`genBinarySearchSkeletonBlock` (stage 4, fully correct O(log n)) sitting
un-forced in the default skeleton pool alongside every other option,
`--speed-bonus-weight 0.0` (default) vs. `0.3`, 5 seeds each, same
budget (`--max-n 8 --beam-width 3000 --candidates 3000 --num-vars 9`).

**Result: identical in both cases -- 4/5 and 5/5 seeds all converge to
the same trivial size-27 O(n) linear scan.** The binary-search skeleton
is never picked up, with or without the speed bonus.

**Root cause, confirmed directly from the logs, and it's not (only) the
size tie-break section 20 identified:** `BeamSearch.cpp:606` advances
(and, per the cascade check right below it, often exits the run
entirely once the cascade confirms across every level) the INSTANT any
candidate reaches a full score on the current level -- it never keeps
searching a level for something faster once anything correct is found.
Every seed's log shows the trivial scan discovered at `candidates =
3002` (the very first mutation batch after the initial seed program) on
LEVEL 1, N=1, immediately cascading to `confirmed_levels 8` and ending
the run right there. A `While`-loop-plus-`If` linear scan is a 3-4
statement program reachable by ordinary unbiased statement generation
almost immediately; the binary-search skeleton needs ONE loop-bias draw
to independently land on this specific skeleton (weight 3 among ~15
options) AND commit 8 coordinated scratch variables in that same
statement -- reliably rarer than stumbling into a correct tiny linear
scan within the same couple thousand candidates. By the time that could
plausibly happen, the run has already exited. `speedBonusWeight` only
ever affects ranking BETWEEN candidates that coexist in the same scored
generation -- it can't help a candidate that never gets the chance to
exist before the run stops.

**This refines, not contradicts, section 20's tie-break argument**:
score+fitness+size tie-break is real and would ALSO block the skeleton
even if both candidates somehow coexisted in one generation, but it
turns out to be moot here -- the dominant, more immediate cause is that
the search stops looking the moment anything correct exists at all, so
the two almost never get the chance to compete in the same generation
in the first place.

**Where this leaves the skeleton**: `genBinarySearchSkeletonBlock`
(stage 4, the validated correct algorithm) stays in the default pool --
same status as every other `gen*SkeletonBlock` here, available for
whatever problem's search actually needs it and reachable via
`ALGOSEARCH_FORCE_SKELETON=binarysearch` for direct verification (see
section 22's stage-4 result: 5/5 genuinely generalizing). For
`contains_ordered` specifically, getting the search to prefer it over
the trivial linear scan by default would need either (a) disabling or
delaying exit-on-first-full-solve so a level stays open for more
competition once solved once -- a real engine change, out of scope for
this investigation -- or (b) starting the search past the trivial-scan-
discovery regime some other way. Not pursued further here; flagged as
an open thread below rather than assumed away.

## 24. Cataloguing the skeleton library: which primitives actually recur, and which are irreducible

Prompted by the binary-search ladder (sections 22-23): if the search is
this dependent on hand-written skeletons -- nothing without one, full
convergence with one -- the "intelligence" claimed for this project is
concentrated in the ~15 `gen*SkeletonBlock`/`gen*LoopBlock` functions in
`ProgramGenerator.cpp`, not in the search mechanism around them. Read
every one of them start to finish and sorted their contents into
primitives, to see how much is genuinely reused vs. how much is
irreducibly one-off per skeleton.

**Primitives that recur across MANY skeletons (the load-bearing, already-
shared vocabulary):**
- **Counting loop** (`vi=0; while(vi<v0){body; vi=vi+1;}`): the single
  most reused shape -- `genCountingLoopBlock`'s five `AccumKind`s,
  `genAverageSkeletonBlock`'s sum phase, `SelectionSort`'s inner/outer
  loops, `ThreeSum`'s three nested loops, `GraphReachability`/
  `ShortestPath`'s i/j loops, TSP's per-permutation cost-eval loop.
- **Captured scalar from `arr[0]` before the loop starts** ("query/
  target capture"): `Membership`/`CountIf`'s target, `NestedDivisor`'s
  `x`, `GcdEuclid`'s a/b seeds, `DtwProblem`'s `n`, TSP's `nv`,
  `BinarySearch`'s `target`.
- **Compare-and-replace running best** (`if (cand < best) best = cand`):
  `Min`/`Max` `AccumKind`, `SelectionSort`'s `mi`, `HeapSort`'s sift
  comparisons, TSP's `best`.
- **Flattened-2D-into-1D indexing** (`base + i*width + j`):
  `GraphReachability`, `ShortestPath`, `DtwProblem`, TSP's edge matrix --
  4 independent skeletons, all hand-deriving the same trick because
  Mini-C has no native 2D array.
- **Division/mod via repeated subtraction** (`while (r>=n){r=r-n;
  q=q+1;}`): `Average`'s divide phase, `GcdEuclid`/`GcdMod`'s inner mod
  loop -- later given a native-primitive sibling (`GcdModNative`,
  `NestedDivisorNative`, section 17) once `Mod`/`Div`/`Mul` existed.
- **Sentinel/done flag scalar**: `ThreeSum`'s `found`, `HeapSort`'s
  `done`, TSP's `valid`, `BinarySearch`'s `found`.

**Primitives that appear in exactly ONE skeleton (irreducible so far --
and notably, every one of these corresponds to a genuinely different
ALGORITHM CLASS, not a different PROBLEM):**
- **Converging two-pointer loop** (`while(lo<hi){...halve the range...}`)
  -- only `genBinarySearchSkeletonBlock`. Confirmed in sections 20-23 to
  never emerge from the shared vocabulary above, at any scaffolding
  level short of writing it whole.
- **Fixed-round relaxation** (`round=0; while(round<n){ nested i,j
  passes }; round=round+1;`) -- `GraphReachability`/`ShortestPath` share
  this one (2 skeletons, not 1), needed specifically because Bellman-
  Ford-style relaxation has no already-correct visitation order.
- **Ordered single-pass DP fill** (i then j strictly increasing, no
  rounds, because every cell depends only on already-computed
  neighbors) -- only `DtwProblem`. Looks superficially like the
  relaxation shape above (both fill a flattened 2D table) but is
  actually a different, cheaper primitive that only works when the
  DP's dependency order is already linear -- confusing the two would
  have been a real bug.
- **Countdown loop + flag-loop-instead-of-break** (`k=n-1; while(k>=0)
  {...}`, and `done=0; while(done==0){...; if (cond) done=1;}`) --
  both only in `HeapSort`'s sift-down, working around Mini-C having
  neither `break` nor a natural way to loop from the top down without
  hand-picking `>=0` as the bound.
- **AND-via-nested-if** (no `&&` in Mini-C) -- only `HeapSort`'s
  two-condition child-index checks; every other skeleton either needs
  no compound condition or expresses it additively (section 9).
- **Iterative permutation enumeration** (Heap's algorithm via an
  explicit counter array, no recursion) -- only TSP brute force.
- **Explicit choice-stack DFS with backtracking** (`path[]`/`used[]`/
  `level` standing in for a call stack) -- only TSP branch-and-bound.
- **Cost-based pruning guard** (`if (cost < best) ...`) -- only TSP
  branch-and-bound, and per section 18's four independent rescue
  attempts, the ONE piece in this entire catalogue that free search
  could never reconstruct even when everything else was handed to it.

**What this says about where the earlier strategic question (expand
categories vs. generalize now) should land:** the skeletons that
LOOK numerous (15) mostly recombine a small, already-reused core of
~6 primitives above -- most of the effort in this project's problem
count came from recombining those, not from inventing new control-flow
each time. The real "new intelligence" moments were rare and cluster
exactly where a problem needed a genuinely different algorithm CLASS
(search/pruning, permutation enumeration, heap operations, two-pointer
convergence, round-based relaxation vs. ordered-pass DP) -- five
distinct one-off primitives across 15 skeletons and ~20 problems. This
directly informs the held-out test discussed above: a new problem whose
natural algorithm reuses the shared core (counting loop + running-best +
flattened indexing, recombined differently) is a real test of whether
decomposing that core into standalone primitives helps composition; a
problem needing a SIXTH one-off primitive (most likely candidate:
classic 1D DP like LIS/coin-change, which needs "read several already-
computed table entries and take a max/min over them," structurally
unlike anything in this catalogue) will almost certainly need a human-
written skeleton regardless, matching TSP/BinarySearch's precedent
exactly.

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
4. ~~Curriculum work for `count`/`average`/`power`~~ -- done (section 10's
   follow-up): `CountIf`, `genAverageSkeletonBlock`, `genPowerSkeletonBlock`
   added, all three now solve. Next: check whether `genAverageSkeletonBlock`'s
   division-via-repeated-subtraction transfers to `mod`/`gcd` if offered as
   a standalone generic primitive instead of baked into one atomic
   sum-then-divide block -- would test whether the composability wall
   diagnosed there (independently-drawn skeletons rarely land on
   consistent variables) is really as hard a wall as assumed, or just
   never given enough attempts.

## 25. Held-out discovery benchmark: three problems outside the skeleton library, and the baseline that freezes the starting line

The plenary session (2026-09-20) approved the technical package "divergence
engine + composite primitives + held-out benchmark" in the order **3 -> 1 -> 2**
(benchmark first, engine second, primitives third), with the no-LLM clause
kept intact (section 17's binding decision). This entry is Etapa 1 of that
plan: the ruler, frozen before any engine/grammar change.

### The three held-out problems

Three new `Problem` subclasses, deliberately NOT in the skeleton library
(`ProgramGenerator.cpp` untouched), each chosen to test a different
construction gap from section 24's catalogue:

- **`lis`** (`LisProblem`): longest strictly increasing subsequence. Flat
  array `[n, values..., n zeros scratch]`. Needs the catalogue's predicted
  SIXTH one-off primitive: read several already-computed table entries and
  take a max over them. Ground truth: host O(n^2) DP.
- **`substring`** (`SubstringProblem`): contiguous substring search. Flat
  array `[n, text..., m, pattern...]`. Needs a recombination of SHARED
  primitives (two counting loops with two captured lengths + a per-start
  match flag) that no existing skeleton assembles.
- **`cycle`** (`CycleDetectionProblem`): linked-list cycle detection on a
  next-pointer array `[head, next...]` (n = null). Needs a genuinely new
  two-pointer shape (Floyd's different-speed pointers, or a pigeonhole
  counting walk) that is not in the catalogue at all.

All three override `supportsMagnitudeShift() = false` (metadata/indices, not
shiftable magnitudes). Unit tests cover instance layout, ground-truth
consistency of every generated instance, and both answer labels; `make test`
68/68 after the addition (58 prior + 10 new).

### Baseline protocol and result

`scripts/run_heldout_baseline.sh`, output in `data/heldout_baseline_20260920/`.
Fixed matched budget for every run: beam=500, candidates=800,
max-program-size=60, step-limit=10000, num-vars=6, max-n=5, 5 seeds (base
2001), 600s wall cap. Two arms per problem: default `loop-bias=0.4`
(skeleton pool available, unforced) and `loop-bias=0.0` (no skeleton draw at
all).

**Result: 0/30 genuinely generalizing solutions.** Every one of the 30 runs
produced only `UNCONFIRMED` gambiarras -- programs that fully solve their
current level's exact instances and break one level later, exactly the
failure mode the structural-invariance gate exists to label. No seed
produced a single `SUCCESS` record in either arm.

Per-problem high-water marks (max level reached / best score at that level):

| problem | arm bias=0.4 | arm bias=0.0 |
|---|---|---|
| lis | N=3 (8-10/14) | N=2 (7-9/10) |
| substring | N=3 (16-17/21) | N=3 (13/21) |
| cycle | N=5 (7/10); one seed fully solved N=4 (10/10) but UNCONFIRMED | N=3 (7/8) |

The five-column taxonomy the plenary adopted, applied to the baseline:

| column | answer |
|---|---|
| solução encontrada? | 0/30 generalizável (todos UNCONFIRMED) |
| é equivalente a algoritmo conhecido? | n/a -- nada sobreviveu à verificação |
| usa somente primitivas conhecidas? | n/a |
| estrutura nova? | n/a |
| comportamento novo? | n/a |

### What this freezes

This is now the definitive starting line for the 3 -> 1 -> 2 plan: the
current engine, with or without the skeleton library, gets zero traction on
these three shapes at a matched modest budget. Etapa 2 (the divergence
engine, Moção 1) and Etapa 3 (composite primitives with binding, Moção 2)
will be measured against exactly this table -- a result only counts as
progress if it moves one of these problems from UNCONFIRMED to an
independently-verified SUCCESS without hand-writing its skeleton.

## 26. Divergence engine (Moção 1, Etapa 2): collects alternatives, still zero generalizers

Implemented `SearchConfig::postSolveSizes` (`--post-solve-sizes`, default 0):
after the first full solve of a level, the level stays open for N more
program-size steps; the champion is frozen (excluded from the beam so elite
slots grow alternatives instead of re-selecting it), every NEW full-solve
candidate found during the window is recorded with its own cascade, and
promotion happens only when the window closes -- to the level reached by the
archived champion whose cascade went farthest (ties: smaller program). 0
reproduces the original promote-immediately behavior exactly. Records are
deduplicated per program@level so an archived champion re-found later cannot
double-count. `make test` 71/71 (3 new tests: default-mode record count is
unchanged, the window records alternative champions, and the window still
promotes after the budget elapses without deadlocking).

**A/B against the section 25 baseline** (`scripts/run_heldout_divergence_ab.sh`,
`data/heldout_divergence_ab_20260920/`): same binary, same matched budget
(beam=500, candidates=800, size=60, step-limit=10000, num-vars=6, max-n=5,
5 seeds base 2001, 600s wall), four arms -- bias 0.4/0.0 x post 0/20.

**Result: 0/30 genuinely generalizing solutions in the post=0 control arm AND
0/30 in the post=20 treatment arm.** The control arm re-confirms section 25's
baseline with the new binary (same max levels and scores), so the comparison
is matched. The window does its mechanical job -- UNCONFIRMED records per run
jump from 1-4 to ~19-29 as alternative gambiarras get archived -- but every
one of those alternatives is another wrong-but-fully-scoring program, and
none generalizes. The trade is also visible: max level reached DROPS in
several treatment runs (e.g. `lis` bias=0.4 solves N=2 in 3/5 control seeds
but only N=1 in 5/5 treatment seeds, per `confirmed_levels`/"solves N=" in
the raw logs -- corrected from an earlier N=3/N=2 misreading of the level the
search was merely attempting, not the level actually solved) because each
open window spends size budget re-exploring the same level instead of
climbing.

**Conclusion, honest negative, correctly scoped:** this specific survival
mechanism -- keeping wrong-but-fully-scoring alternatives alive in a post-
solve window -- did not produce construction of any generalizing new shape,
in this search space, under this generation operator. That is narrower than
"the bottleneck is construction, not survival" (the earlier phrasing):
this experiment does not rule out every possible survival strategy, only
this one. Combined with sections 18/20/21, it is nonetheless the fourth
independent confirmation that widening what the beam keeps alive, by itself,
does not create the missing shape. The engine change stays (opt-in, default
off, same status as `speedBonusWeight`/`exprMutationRate` before it): the
archive semantics are exactly what the composite-primitives stage (Etapa 3,
Moção 2) will need to tell "recombined something new" from "re-found the
same gambiarra". But by itself, on the held-out ruler, it is a clean null
result.

## 27. Composite primitives with binding (Moção 2, Etapa 3): the operator composes the shape, but the search never converges to the clean version

Two-layer success criteria, fixed BEFORE any code (per the plenary audit):
- **Test A (composition) = `substring`**: its solution is a NEW recombination
  of section 24's catalogued primitives (two counting loops with two captured
  lengths + a per-start match flag) in an arrangement no existing skeleton
  assembles. Success here = the search composes it unaided; record as
  "composition learned", never as "discovery".
- **Test B (open discovery) = `lis`**: needs the catalogue's predicted SIXTH
  one-off primitive (read several already-computed table cells and take a
  max), which the composite family cannot express by construction. Expected
  to fail; a failure is the measured frontier of "primitives + binding" and
  is direct input for Moção 4, NOT a reason to add an ad-hoc LIS primitive.
- `cycle` is the reserve (a new two-pointer shape), reported separately.

Implemented `GenConfig::compositeBias` (`--composite-bias`, opt-in, default
0.0): at a generic statement slot with 2 free loop slots, emits a two-loop
block assembled from catalogued pieces -- capture `arr[0]`, capture at a
computed position (`arr[arr[0]+k]`), two counting loops with derived bounds,
affine two-sequence indices (`base1+i+j` vs `capA+base2+j`), and a relation
piece (align-match sentinel / count-if / xor / running best) -- under ONE
shared binding context (`pickDistinctVars` once; every piece uses those
variables, never fresh independent draws). This is deliberately a FAMILY of
composites (the substring solution is one coherent draw among ~50), not a new
named skeleton. Original grammar tokens stay available (the operator is one
trigger among many, never a replacement). `make test` 72/72.

**Three iterations, one real bug found:** v1's coherent outer bound was
`n - m`; the first A/B showed the operator composing the right shape with the
wrong bound (champion: outer `v5 < v2` = n, which over-counts and creates
false positives). A coherence prior (v2) weighted the aligned choices but the
bound was still wrong. v3 fixed an off-by-one -- the number of valid start
positions is `n - m + 1` (`(capA - capB) + 1`), not `n - m`; the v1/v2
champions were one constant away from correct. This is the same
hand-authored-template fragility the audit warned about, found and fixed
before the final A/B.

**Final A/B** (`scripts/run_heldout_composite_ab.sh`,
`data/heldout_composite_ab_v3_20260920/`): loop-bias=0.0 (no skeleton draw at
all), matched section 25 budget, composite-bias in {0, 0.5, 1.0}, 5 seeds.

| test | composite=0 | composite=0.5 | composite=1.0 |
|---|---|---|---|
| Test A `substring` | 0/5; N=2-3, best 13/21 | 0/5; N=3-4, best 23/29 | 0/5; N=3-4, best 24/29 |
| Test B `lis` | 0/5; N=2, best 9/10 | 0/5; N=2-3, best 11/14 | 0/5; N=2-3, best 11/14 |
| reserve `cycle` | 0/5; N=1-3 | 0/5; N=2-3 | 0/5; N=1-2 |

**What the operator did and did not do:** the binding discipline WORKS. The
v3 champions at N=3 contain the exact coherent composite -- `while (v2 <
((v5 - v3) + 1)) { ... if (arr[(1 + (v2 + v1))] == arr[(v5 + (2 + v1))]) ... }`
-- i.e. the operator composed the target primitive arrangement, including the
correct `n-m+1` bound, without a human writing the algorithm. But every such
champion is a 100+-node overgrown program (growth spliced extra composite
blocks into the body), scores full only on the current level, and fails the
structural-invariance gate one or two levels later.

**Isolation test (`tests/composite_isolation_tests.cpp`), corrected reading:**
the plenary's working hypothesis after the A/B was "construction succeeded,
convergence to a clean version did not" -- i.e. a selection/growth problem.
Hand-writing the CLEAN version of what the catalog's pieces can assemble
(no growth corruption at all) disproves that hypothesis. The catalog's
"align-match sentinel" relation piece is `if (arr[textIdx] == arr[patIdx])
acc = 1`, fired unconditionally inside the inner loop -- a flat OR over
every `(i, j)` pair. `substring` needs an AND over the whole inner window
for a FIXED `i` (a per-outer-iteration flag that starts true, clears on any
mismatch, and is only compared/OR'd into the accumulator after the inner
loop closes). The catalog has no piece with that reset-before/compare-after
shape. `catalog_only_composite_does_not_generalize_even_when_clean` confirms
the clean catalog-only composite fails by level 4, with **zero** growth
corruption -- the champions' 100+-node bloat was very likely the search's
attempt to compensate for the missing piece with ad-hoc structure, not
incidental noise on top of an otherwise-correct program.

A second test, `true_substring_algorithm_generalizes_and_is_reachable_by_the_interpreter`,
hand-writes the actually-correct algorithm (with the reset+AND+compare-after
piece) and confirms it generalizes exactly across levels 1-8, instances and
adversarial instances alike, at 62 nodes -- so the shape IS within the
interpreter's expressive power. The gap is specifically in the composite
catalog's piece inventory, not in what Mini-C can express.

**Revised verdict for Test A: not "partial via selection failure" but a
construction gap of the same KIND as Test B, only smaller** -- `substring`
needs one piece (reset-scoped conjunction) the catalog does not have, exactly
as `lis` needs a piece (read-several-cells-and-max) the catalog does not
have. Both are catalog-completeness gaps, not survival/selection gaps. This
sharpens sections 18/20/21/22's repeated finding: every mechanism tested so
far that only changes what survives (diversity, crossover, niches,
annealing, the Moção 1 archive) or how existing pieces recombine (Moção 2)
leaves the actual bottleneck untouched -- the grammar/catalog has to contain
the needed piece before any search mechanism can find it.

Test B failed as predicted (the composite family has no "read several
computed cells and take a max" piece, so `lis` stayed out of reach by
construction) -- direct input for the Moção 4 discussion. `cycle` likewise
stayed out of reach.

**Next actions, in order:** (1) DONE -- isolation confirmed the gap is in the
catalog's piece inventory, not selection/growth; (2) do NOT spend further
effort tuning `exprMutationRate`/growth-granularity/the Moção 1 archive
against this specific case -- that lever was ruled out by the isolation
test, not just deprioritized; (3) if a piece with the reset-then-conjunction
shape were added to the catalog, `substring` would move from "construction
gap" to "the same open question sections 18/20/21 raise for every other
solved skeleton": would the search actually recombine it into the right
place, or does adding it become one more skeleton by another name? That
question is deliberately NOT tested here -- it would require designing the
new piece carefully enough to stay general (usable by other future
held-out problems) rather than shaped specifically for `substring`, which
is exactly the "17 skeletons vs. 6 new ones" trap the plenary flagged before
Etapa 3 began. The composite operator stays opt-in (default 0.0).

## 28. Diversity battery: does the combination of both new mechanisms increase archived diversity across the ORIGINAL 21, and does it regress anything?

Etapas 2 and 3 measured `--post-solve-sizes` and `--composite-bias` only
against the 3 held-out problems, in isolation from each other. Open
question after the session: turned on TOGETHER, across the *original*
skeleton-solved problems (where the search already generalizes), does the
divergence window collect more distinct alternative programs, and does it
cost anything?

**Setup** (`scripts/run_diversity_battery.sh`, `data/diversity_battery_20260920/`):
13 problems -- 10 from the original 21 (`sorting`, `min`, `max`, `sum`,
`gcd`, `is_prime`, `factorial`, `contains`, `three_sum`,
`graph_reachability`) plus the 3 held-out (`lis`, `substring`, `cycle`).
Two arms, matched budget/seed: **control** (today's default, both flags
off) vs. **treatment** (`--post-solve-sizes 15 --composite-bias 0.3`
together). beam=300, candidates=800, size=80, step-limit=4000, loop-bias=0.4,
3 seeds/arm, 90s wall cap.

### Result 1: diversity rises sharply, generalization is UNCHANGED

For the 10 runs that completed (see the bug below for the 3 that didn't),
distinct archived `program_id` count per run and the union of levels that
reach confirmed `SUCCESS`:

| problem | distinct programs, control -> treatment | SUCCESS levels, control | SUCCESS levels, treatment |
|---|---|---|---|
| sorting | 2 -> 32 | 2-5 | 2-5 (identical) |
| min / max | 2 -> 32 | 2-8 | 2-8 (identical) |
| sum | 2 -> 32 | 2-8 | 2-8 (identical) |
| contains | 1 -> 16 | 1-8 | 1-8 (identical) |
| gcd | 1 -> 16 | 1-15 | 1-15 (identical) |
| three_sum | 3 -> 48 | 3-6 | 3-6 (identical) |
| graph_reachability | 2 -> 48 | 3-5 | 3-5 (identical, control also reaches 2 but treatment's first SUCCESS starts one level later -- see caveat below) |
| lis (held-out) | 1.3 -> 18.7 | none | none (as expected, section 27) |
| substring (held-out) | 1.3 -> 16 | none | none (as expected, section 27) |

Spot-checked (`max_treatment_seed3001`) that the extra `program_id`s are
genuinely distinct program TEXT, not a hashing artifact (e.g. `v1 = v0;`,
`v1 = 2;`, `v3 = v1;`, `v3 = v2;` -- different trivial level-1 candidates,
all legitimately different programs).

**Caveat on `graph_reachability`:** control's SUCCESS union is `[2,3,4,5]`
in the aggregate table above but the per-seed detail shows the divergence
window can shift WHICH level a given seed's cascade starts confirming from
by one, consistent with section 26's observed cost (the window spends size
budget re-exploring instead of climbing) -- worth a closer per-seed look
before calling this cost-free at scale, but no run's FINAL reachable level
regressed.

### Result 2: a genuine hang bug, found only by this broader sweep

`is_prime`, `factorial`, and `cycle` -- the only three problems in this
batch run with `--num-vars 5` -- **timed out on every one of their 9
treatment-arm runs** (3 problems x 3 seeds), producing zero output (not
even a flushed level header) after the full 90s wall cap. All 9 corresponding
control runs finished in under 3.1s.

**Isolated with direct reproduction** (`./algosearch --problem is_prime
--num-vars 5 ... --composite-bias 0.3 --post-solve-sizes 15`, no wall cap,
15s timeout): hangs deterministically, zero bytes of output even
unbuffered. Bisected the trigger:

| composite-bias | post-solve-sizes | num-vars | Result |
|---|---|---|---|
| 0.3 | 0 | 5 | OK |
| 0 | 15 | 5 | OK |
| 0.3 | 15 | 6 | OK |
| **0.3** | **15** | **5** | **HANGS** |

**Conclusion: a specific, reproducible interaction bug between
`--composite-bias` and `--post-solve-sizes`, triggered only at exactly
`--num-vars 5`.** Root cause not yet isolated to a line of code, but the
prime suspect is `genCompositeBlock`'s `pickDistinctVars(rng, cfg, 5)`
call (`ProgramGenerator.cpp`): at `numVars == 5` it must consume EVERY
available variable with no slack, which combined with the divergence
window's requirement to keep finding a NEW (textually different) champion
every extra size step plausibly starves some retry/dedup loop of any
variable-assignment freedom to try. This is untested against the codebase's
other tight-numVars problems and is not yet fixed.

**Status: both flags remain opt-in and default off, so no existing default
run is affected.** But this rules out recommending the two flags be used
together on any `--num-vars 5` problem until fixed, and is a blocking
prerequisite for extending the divergence-window + composite-operator
combination to the rest of the original 21 (several of which use
`num-vars` values in this range).

**Next action:** root-cause and fix the hang (suspect `pickDistinctVars`
under composite-bias interacting with the divergence window's frozen-
champion dedup), then re-run `is_prime`/`factorial`/`cycle` in this battery
before drawing any conclusion about diversity gains for `num-vars 5`
problems specifically.

### Where the diversity increase actually comes from

Mechanistically, distinct from any new "creativity": the divergence window
(`SearchConfig::postSolveSizes`, section 26) is the primary source. Once a
level's first full solve happens, the window keeps the level open for N
more program-size steps, excludes the champion from the beam, and records
EVERY subsequent NEW full-solve program (deduplicated by `program@level`)
as a separate archive entry instead of promoting immediately. Composite-bias
(section 27) adds a second, smaller contribution: because it injects a
whole new family of two-loop statement shapes into `genStmt`'s draw
distribution (independent of the skeleton draw), more of the size-growth
mutations happening during that same open window land on structurally
different programs than the ordinary grammar alone would produce -- so the
window has a wider variety of shapes available to find and archive, not
just more time to re-find variations of the one shape the skeleton grammar
already favors. Both pieces were already known: this battery's contribution
is confirming the same compounding effect holds across the ORIGINAL 21
problems, not just the 3 held-out ones -- and that, for the 10 problems
where both flags run cleanly, it costs nothing in final generalization.

## 29. Long-budget sorting run: real structural transition, inflated diversity counts, and a growth-corruption stall

A ~20h-budgeted `sorting` run was attempted, combining `--post-solve-sizes 150`
`--composite-bias 0.15` with the pre-existing diversity/recursion machinery
(`scripts/run_sorting_20h.sh`, `data/sorting_20h_20260920/`), to see how many
structurally distinct sorting algorithms accumulate under a large budget.
Two infrastructure bugs surfaced and were fixed before the run that produced
the data below (both worth recording, since either would silently corrupt
any future long-budget run):

1. **`--program-size` is a single counter for the ENTIRE run, not reset per
   level** (`BeamSearch.cpp:303`: `for (int size = 1; size <=
   config_.maxProgramSize && currentLevel <= config_.maxLevel; ++size)`).
   With `--post-solve-sizes 150` and `--max-program-size 300`, only ~1.5
   divergence windows fit in the whole run before the outer loop's size
   condition failed -- the search then exited NORMALLY (`exit=0`, looks like
   a clean finish) after only 2 levels. `--max-program-size` must scale with
   `(number of levels expected to complete a divergence window) x
   (post-solve-sizes)`, not with the complexity of any single program.
2. **`--benchmark` is a different code path** (`runBenchmarkMode` in
   `main.cpp`) that runs `--benchmark-seeds` independent short verification
   searches and never touches `--output`/`--checkpoint-path` at all --
   passing it alongside a long exploration run silently discards the
   intended search entirely. Scripts for long single-run exploration must
   NOT pass `--benchmark`.

Also added this session: `SearchConfig::checkpointPath` /
`--checkpoint-path` (`BeamSearch.hpp/.cpp`, `main.cpp`), which overwrites
the solutions file at every level promotion instead of only on natural
completion -- the engine has no internal wall-clock cutoff (section 8), so
an external `timeout`/`kill` previously lost everything. 2 new tests
(`checkpoint_path_writes_progress_before_the_run_completes`,
`empty_checkpoint_path_writes_no_file`), `make test` 76/76.

### What the corrected run actually found (levels 1-38, killed after stalling -- see below)

- **Levels 2-31: the classic selection-sort skeleton, repeatedly.** 8
  distinct `program_id`s each generalize across all 30 levels (structural
  invariance confirmed, labeled `SUCCESS`), but inspecting the program TEXT
  shows they are the SAME selection-sort core (`for i: find min in [i,n),
  swap`) with cosmetic variable renaming and inert dead-code tails (`v3 =
  v4; v3 = v4;`, an `if` that never fires) appended by the divergence
  window's growth. The archive's `program_id` dedup (by exact program text)
  counts these as 8 "distinct" solutions; they are ONE algorithm.
- **Levels 32+: a genuine structural transition.** Programs acquire
  `call(...)` (recursion, gated by `--recursion-call-bias`) and `aux[...]`
  (a second array, gated by `--second-array-bias`) -- the classic
  merge-sort/divide-and-conquer signature -- and a level-36 sample is
  recognizably **heap sort** (`v2 = (v7 + v7) + 1` is the standard
  `2*i + 1` heap-child index, with sift-down comparison/swap logic). Coarse
  structural signatures (counts of `while`/`if`/`swap`/`call`/`aux[`, ignoring
  variable names and literals) across the 152 distinct `SUCCESS` programs
  found 106 distinct coarse shapes -- real diversity, not cosmetic, starting
  specifically where recursion/second-array become reachable.
- **Raw counters overstate diversity substantially.** 1376 total records /
  661 distinct `program_id`s sounds like massive diversity; the honest count
  of functionally distinct algorithm FAMILIES actually present is closer to
  2-3 (selection sort; a heap-sort-like family; whatever the many
  `call`+`aux[` variants at level 32+ collapse to once de-duplicated by
  behavior, not text) -- most of the raw count is dead-code padding on
  repeats of the same shape, consistent with sections 26-28's repeated
  finding that the divergence window archives quantity, not algorithmic
  variety, by default.

### The run stalled at N=39 for 8+ hours -- growth corruption, live

`LEVEL 39` never progressed: `best score = 4/48` held constant for the
entire observed stall, while `best size` crept from ~1600 to ~1650 nodes
with zero score improvement. This is section 27's "growth corruption"
finding, caught live and at much larger scale: after enough stacked
divergence windows, the champion program accumulates so much bloat that
further mutation can no longer find ANY working candidate for the next
level, and the search has no escape mechanism (no simplification pass, no
size-based penalty, no restart) -- it simply spins at the same score
indefinitely, burning the entire remaining wall budget for zero information.
Killed manually after 8+ hours with no progress; the `--checkpoint-path`
guarantee held exactly as designed -- levels 1-38 (1376 records) survived
the kill intact.

**Conclusion:** post-solve-sizes' archive is a real, working mechanism for
surfacing structurally different algorithm families when the grammar CAN
reach them (recursion/second-array unlocking merge/heap sort here) -- but it
has no ceiling on program bloat, and unbounded growth eventually kills
forward progress entirely rather than degrading gracefully. A future
divergence-window run should either cap cumulative growth per champion (not
just per window), or add a periodic simplification/re-seeding step when a
level's score stalls for N consecutive size-steps -- neither exists today.
Until then, long-budget runs using `--post-solve-sizes` should be treated as
"run until it stalls, then stop and inspect," not "run for a fixed wall
budget unattended."

## 30. Why `genBinarySearchSkeletonBlock` never gets picked up unforced: two real generator bugs found and fixed, one probability wall left standing

Direct follow-up to sections 20-24's open question (`contains_ordered` never
finds anything sub-linear, and stage 4's fully hand-written skeleton stays
in the default pool but is never selected). Prior instrumentation only ever
looked at the WINNING candidate per run; this time the skeleton's actual
draw rate was measured directly.

**Instrumentation added** (`ProgramGenerator.cpp`, gated behind
`ALGOSEARCH_DEBUG_SKELETON_COUNTS`, same non-production status as
`ALGOSEARCH_DEBUG_LANDSCAPE`/`ALGOSEARCH_DEBUG_ISLANDS`): two atomic
counters, incremented every time the `loopAccumulatorBias` trigger fires at
all, and every time it specifically lands on `BinarySearch`.

**First result, surprising:** the skeleton is NOT rare. At `--num-vars 9`,
one `contains_ordered` run drew it **16,116 times out of 145,681** total
skeleton-trigger events (~11%, matching the weighted-roll arithmetic:
weight 3 out of ~34 total, times `loopAccumulatorBias=0.4`). Raising
`--num-vars` to 24 (far more headroom than the skeleton's 8 reserved
scalars need) changed the draw count only mildly (12,263/119,781) and
**changed the survivor count not at all: zero**, in both cases. This rules
out variable-name collision between the skeleton's 8 reserved scalars and
sibling statements as the cause.

**Root cause #1, found by comparing against the one known-working
instance:** section 22's forced stage-4 run (5/5 genuinely generalizing)
has the skeleton as the ENTIRE program body, alone, no siblings. Ordinary
free generation builds a block of up to `maxStatementsPerBlock` (default 4)
statements, and the skeleton -- like every `gen*SkeletonBlock`/
`genCompositeBlock` -- is just one candidate for one of those slots. Any
sibling statement generated after it in the same block has a good chance
of doing `arr[0] = ...` (the single most ordinary "write the output"
idiom in this grammar), silently clobbering the skeleton's already-correct
result. **Fix:** `genBlockStmts` now stops adding further statements to a
block the instant one of them is `StmtKind::Block` -- the one AST shape
every dedicated skeleton (and only skeletons) returns, confirmed by reading
every `case 0..4`/`default` branch of `genStmt`'s ordinary paths, none of
which ever return `Block`. Purely corrective: a terminal skeleton never
depended on a not-yet-generated sibling, so nothing that mattered is lost.

**Root cause #2, found after #1 alone still gave zero survivors (14,242
draws, 0 survivors):** the skeleton draw was also reachable while
generating the body of an `if` or an unrelated `while`. A self-contained
"compute the whole answer" skeleton gated behind a random condition only
produces the right answer on whichever instances satisfy that condition;
nested inside an unrelated loop, it only runs however many times (0, 1,
many) that loop's own random condition/counter happens to iterate. Either
way it can never reach a full score. **Fix:** the skeleton-draw trigger now
requires `!insideIf && !insideWhile` (both threaded through `genStmt`
already, no new plumbing needed) -- only fires in a context guaranteed to
execute exactly once.

**Verification:** `make test` 87/87 after both fixes. Regression check --
`ALGOSEARCH_FORCE_SKELETON=binarysearch --num-vars 9 --loop-bias 1.0`
(section 22's exact forced protocol) still gives 5/5 genuinely
generalizing, now benchmarked as O(1) (`exponent ~0.19-0.20`) at the sizes
tested rather than section 22's O(log n) label -- same correct algorithm,
tighter fit. Draw counts dropped as expected as each guard landed (16,116
-> 11,012 -> 6,887 across the two fixes), confirming both guards are
actually excluding the contexts they target, not silently no-op-ing.

**Residual result: still negative.** 11 unforced seeds total after both
fixes (3 at `--num-vars 9 --max-n 8`, 8 more at `--num-vars 9 --max-n 20
--loop-bias 0.6`, `--post-solve-sizes 30 --speed-bonus-weight 0.3`
throughout) -- zero produced a genuine binary-search survivor. One
`SUCCESS` candidate in the batch contained `/` (division), size 40, but
inspection showed a decorative dead `(0/2)` expression inside the ordinary
linear scan, not the skeleton's shape -- a reminder that "contains a `/`"
alone is not sufficient evidence and every candidate flagged this way
needs to be read, not just grepped.

**Where this leaves it:** both fixes are genuine, generic corrections (not
gated behind a new opt-in bias, since sibling-clobbering and
conditional/looped execution of a self-contained skeleton can only ever
hurt correctness for ANY problem using ANY dedicated skeleton, never help)
-- they should quietly improve every other skeleton-dependent problem's
construction reliability too, not just `contains_ordered`. But removing
both construction bugs was not sufficient on its own to flip the outcome
here: the residual probability of a clean draw arriving AND surviving
`BeamSearch`'s promote-on-first-full-solve behavior (section 23) within a
~3000-candidates-per-size-step budget is still too low across the 11 seeds
tried. The next honest step, if pursued, is a budget an order of magnitude
larger (`--candidates`/`--beam-width` in the tens of thousands, not
thousands) rather than further bug-hunting in the generator -- the
construction path is now clean; what's left is a probability-mass problem.

Code: `src/search/ProgramGenerator.cpp` (`genBlockStmts`'s
`StmtKind::Block` early-stop; the skeleton trigger's `!insideIf &&
!insideWhile` guard; `ALGOSEARCH_DEBUG_SKELETON_COUNTS` instrumentation).
Data: `data/binary_search_gap_20260921/`, `data/skeleton_fix_20260921/`.

## 31. Substring isolation test: the gap is compositional (search), not expressive (VM), same two-phase protocol as TSP/DTW

`SubstringProblem.hpp`'s own comment states a design-time hypothesis that
was never actually checked: the naive O(n*m) substring-match shape
"recombines only the project's SHARED primitives (counting loop +
captured scalar + sentinel flag) in an arrangement no existing skeleton
builds" -- kept out of the skeleton library on purpose as a held-out
ruler for compositional generalization. Ran the same two-phase isolation
protocol sections 18-19 used for TSP/DTW to check that hypothesis for
real, prompted by a batch sweep (`data/heavy_battery_20260921_1848/`)
where `substring` (along with `shortest_path`, `tsp`, `dtw`, `lis`) timed
out at 30 wall-clock minutes with no solution -- initially suspected as a
`--num-vars` misconfiguration like `tsp`/`dtw` (see section 30's sibling
fix in `main.cpp`'s new `resolveNumVars`), but `substring` has no
dedicated skeleton at all, so no `numVars` gate applies to it -- the
timeout needed its own diagnosis.

**Phase 1 -- can the VM execute the correct algorithm at all?** Hand-built
the naive nested-loop program directly in AST form (`tests/
substring_isolation_tests.cpp`), using *only* primitives every other
skeleton already relies on (`Assign`/`If`/`While`/compare/arithmetic/
array access -- deliberately no `Break`/`Continue`, to keep the
comparison to what free search's own vocabulary already contains):
finds `n`/`m`/`patternBase` from the flat-array encoding, then for each
start position `i` in `[0, n-m]` walks `j` in `[0, m)` comparing
`text[i+j]` to `pattern[j]`, clearing an `ok` flag on any mismatch, and
setting a `found` flag when a full window matches. Three tests, 90/90
total suite: hand-picked cases (front/middle/back match, absent,
single-element, pattern-longer-than-text guard), all of
`SubstringProblem::generateInstances` for N=1..8, and all of
`generateAdversarialInstances` (wide-range values, negatives) for N=2,4,
6,8. **Result: passes every case.** This rules out the VM/interpreter as
the bottleneck -- the algorithm is squarely inside Mini-C's expressive
range with the existing primitive set.

**Phase 2 -- can free search compose it from the same catalog?** Ran the
real search with no dedicated skeleton (none exists for `substring`),
`--num-vars 12` (matching phase 1's scratch-register count, so
insufficient registers can't be the excuse), generous budget
(`--beam-width 1000 --candidates 3000 --max-program-size 200 --step-
limit 20000 --seed 1000`), N=1..8. **Result: negative, and not narrowly
-- the search never even gets close to the right shape.** What it finds
at each level it manages to fully solve is a different kind of
wrong: at N=1 a **verbatim selection-sort fragment** (score 4/4, only
because moving the array's minimum to position 0 happens to satisfy
`output[0]`'s label by coincidence at that tiny N); at N=3, after ~700
rounds plateaued at 17/18, an eventual 18/18 "solution" that is a
**156-node grab-bag** mixing selection-sort remnants, XOR-indexed array
writes, and binary-search-shaped arithmetic bolted together via
degenerate expressions like `arr[((1 + v4) ^ (v1 + 3))]` and
`v0 = (arr[3] / (v3 - arr[v11]))` -- syntactically valid, numerically
tuned to that N's 18 instances, held-out 8/26 (fails immediately outside
training). Both are flagged `UNCONFIRMED (fails structural invariance)`
by the search's own promotion check before this run's script ever
inspected them -- the search itself already knows these aren't real
algorithms. By N=4 (29 instances) the search plateaus at 26/29 and the
run ends (170s) without ever producing a full solve at that level, let
alone anything resembling the ok-flag/mismatch-per-position structure
phase 1 confirmed the VM can run.

**Conclusion, matching the TSP/DTW precedent exactly:** the VM can
execute the target algorithm (phase 1, unambiguous), but free
recombination of the existing primitive catalog does not find it (phase
2, unambiguous) -- what appears instead is memorization artifacts
(selection-sort reuse, index arithmetic tuned to one N's instances) that
happen to clear the binary pass/fail bar at small N without encoding the
actual match logic. This is the same "no correct-and-selective neighbor
nearby in expression-space" failure mode section 18 described for TSP's
`cost < best` guard: a per-position "compare all j, clear a flag on any
mismatch, only commit found on a clean sweep" idiom sits in a part of
program-space generic mutation doesn't wander into, even with ample
`numVars`, budget, and 8 clean training levels to fit against.
Practical implication, same as section 18's: `substring` needs a
hand-written `genSubstringSkeletonBlock` (nested counting loop +
captured `ok` flag, mirroring `genDtwSkeletonBlock`'s two-phase-verified
approach) if it's meant to be solvable rather than serve purely as the
held-out compositionality ruler its own doc comment already names it as.

Kept: `tests/substring_isolation_tests.cpp` (3 tests, phase 1's
hand-written reference implementation, permanently regression-tested).
Not yet built: the skeleton itself -- phase 2's negative result is the
diagnosis, not the fix. `make test` 90/90 after adding the isolation
tests.

Data: `data/heavy_battery_20260921_1848/log_substring.txt` (the original
timeout), free-search log referenced in this section's own investigation
(not committed as a `data/` battery -- a single diagnostic run, not a
sweep).

## 32. Scoped conjunction: adding the missing piece as a family capability, not a substring scaffold -- positive on two different problems

Section 31 confirmed the gap for `substring` is a catalog-completeness
gap (VM can run it, the composite catalog cannot compose it): the
catalog's three relation pieces (sentinel-flag/count-if/xor, from section
27) all fire unconditionally once per `(i, j)` pair inside the inner
loop, and none has the RESET-before/AND-reduce/CONSUME-after scope
discipline `exists i, forall j: condition(i, j)` needs. Implemented that
as a fourth relation piece, `genCompositeBlock`'s "scoped conjunction":
a per-outer-iteration flag (`acc`) resets to true before the inner loop,
clears on any inner mismatch, and is only OR'd into a persistent `found`
register AFTER the inner loop closes -- deliberately built as one more
member of the existing relation-piece family (same shared binding
context, same affine `textIdx`/`patIdx` index forms as the other three),
not a `genSubstringSkeletonBlock`. The mismatch predicate itself is also
a draw (`Ne` or `Le`, 50/50), so the piece isn't hard-coded to
substring's own equality test either. Needs a 6th binding variable
(`found`); the shared context grew from 5 to 6 vars.

**A real bug found immediately: infinite loop, not just a wasted-budget
gate.** `pickDistinctVars` draws from a pool of size `numVars - 1` (it
never returns `v0`); bumping the shared context to 6 vars without
re-checking the trigger's `numVars >= 6` gate left `numVars=6` exactly
one short of what 6 *distinct* draws need, and `tests/generator_tests.cpp`'s
`composite_bias_emits_bound_nested_loops` (which used `numVars=6`) hung
for **7.5+ hours of CPU** before this was caught by the user noticing
`make test` was still "grinding" long after it should have finished.
Root-caused via `ps` (one thread pinned at 99% CPU inside
`algosearch_tests`) rather than by reading code first -- the honest
lesson here is procedural, not algorithmic: after touching generator/
search code, run `make test` under an explicit `timeout` from then on,
since a bug in this layer can hang rather than merely fail. Fixed by
raising the composite-bias trigger's gate to `numVars >= 8` (real
headroom, not the minimum that happens to work) in both
`ProgramGenerator.cpp` and `main.cpp`'s `resolveNumVars` (which now also
auto-raises `numVars` to 8 whenever `--composite-bias > 0`, mirroring the
TSP/DTW/shortest_path fix earlier in this same session -- see the
`resolveNumVars` comment), and bumping the existing test's `numVars` to 8.
`make test` (under `timeout`) 92/92 afterward.

**The family test, run before trusting the result on `substring` alone:**
per this session's own working agreement (echoing the plenary discipline
in sections 26-28) -- a new piece that only ever helps the ONE problem it
was diagnosed against is indistinguishable from a disguised skeleton.
Added `DominatedWindowProblem` (`dominated_window`, `src/problems/
DominatedWindowProblem.{hpp,cpp}`): identical flat two-sequence encoding
and index arithmetic to `SubstringProblem`, but a different `forall j`
predicate -- `text[i+j] > pattern[j]` (strict elementwise dominance)
instead of equality -- chosen specifically to reuse the shape while
forcing the search to use the `Le`-mismatch draw, not the `Ne` one tuned
for substring.

Ran both problems the same way: `--loop-bias 0.0 --composite-bias 1.0
--num-vars 8`, no skeleton draw at all possible (loop-bias governs the
named-skeleton path exclusively), so any solution found is composed
purely from the relation-piece family. **Both converged to the clean,
correct shape immediately** (well under a second, `--max-n 6`):
- `substring`: first_level 3, confirmed_levels 4, held-out 33/33 at N=6,
  outer bound the aligned `(n-m)+1` -- the exact correct bound.
- `dominated_window`: first_level 2, confirmed_levels 5, held-out 33/33
  at N=6, correctly used the `Le` mismatch draw (`arr[textIdx] <=
  arr[patIdx]` clears the flag, matching "dominance requires strictly
  greater").

Both re-confirmed under `--benchmark --benchmark-seeds 5`: **5/5
genuinely generalizing solutions for each, 0 rejected**, independently
re-verified (not just the search's own SUCCESS label), both fit at
empirical exponent ~1.9-2.0/r²=1.00 (correct O(n·m) quadratic).

**One honest caveat, not swept under the rug:** `dominated_window`'s
benchmarked champion used the outer bound `i < n` (the catalog's "plain
capA" option), not the aligned `i < (n-m)+1` `substring` converged to --
looser than necessary (lets `i` run past valid start positions into the
region where `textIdx` reads into the pattern's own encoding slots
instead of past the true array bounds). It nonetheless passed every
training, held-out, and adversarial instance across 5 independent
benchmark seeds. Plausible reading: for this problem's specific value
ranges, an over-run window's spurious comparison happens to resolve
false often enough that it never manifested as a counted failure in
what was tested -- the same kind of "passes everything tried, still not
proven" gap section 27's off-by-one bugs warn about. Recorded here
rather than either asserting it's definitely a hidden bug or definitely
fine; a targeted adversarial case constructed to break exactly that
bound (an over-run window whose spurious comparison would spuriously
dominate) is the natural follow-up before trusting this specific
champion program, not the piece in general.

**Conclusion: this is a genuine composed-capability gain, not a
disguised scaffold.** Same generic piece (reset/AND/consume plus a
2-way mismatch-predicate draw), zero problem-specific code, solved two
structurally-identical-but-semantically-different members of the
`exists i, forall j` family on first contact, both independently
verified. This directly answers the open question sections 18/27 left
standing ("if a piece with the missing shape were added, would the
search actually recombine it into the right place, or does adding it
become one more skeleton by another name?") -- for this piece, on this
small family, the answer is recombination, not another skeleton.
`compositeBias` stays opt-in (default 0.0), same status as every other
research lever in this file.

**Not yet done, natural next steps:** (1) the outer-bound caveat above;
(2) a third/fourth family member testing a genuinely different index
pattern (e.g. a single-sequence window predicate, `arr[i+j]` compared
against `arr[i]` rather than a second captured sequence) to find where
this specific piece's generality actually ends -- expected to fail by
construction, since `patIdx` is currently always affine in the SAME
`capA`/`base2` captured at the top, never re-derived from the outer
loop's own `vi`; (3) folding `dominated_window` into `main.cpp`'s
`recommendedNumVars` table / a real skeleton is explicitly NOT the
point -- it exists only as this section's family-test ruler, same
status as `substring` itself.

Code: `src/search/ProgramGenerator.cpp` (`genCompositeBlock`'s scoped-
conjunction relation, 6-var binding context, `numVars >= 8` gate),
`src/main.cpp` (`resolveNumVars`'s composite-bias case),
`src/problems/DominatedWindowProblem.{hpp,cpp}`, `tests/
held_out_problems_tests.cpp` (`dominated_window_problem_*`),
`tests/generator_tests.cpp` (`numVars` fix). `make test` 92/92.

## 33. Generation-time pruning of text-level trivia (phases 0+1): real junk, removing it is neutral

**Motivation.** `canonicalText()` (`src/ast/Stmt.cpp`) already folds
constant conditions, strips dead stores via real backward liveness and
renames variables canonically -- but only to compute the dedup hash/program
id. The program that stays in the beam is never simplified, and the
generator itself had no check against emitting trivially useless shapes.
This section is phases 0 and 1 of a five-phase pruning plan (0: measure;
1: redraw trivia at generation time; 2: algebraic expression folding;
3: materialize the simplification on beam programs; 4: behavioral dedup
with a per-class cap; 5: stall-triggered simplification, section 29's
suggestion).

**Implementation.** `GenConfig::pruneGeneration` / `--prune-generation`
(opt-in, default off). Pruned rules, each checked after the site is built
and redrawing only the offending part (at most 8 times):
`v = v`; `arr[e] = arr[e]` / `aux[e] = aux[e]`; `swap(e, e)`; an if/while
condition constant by its text (same `constantConditionValue` the
canonicalizer uses, now public in `ast/Expr.hpp` alongside `exprEqual`);
Div/Mod by literal 0 and Shl/Shr by a literal outside [0, 31]. The check is
post-construction on purpose: `makeBinOp(k, gen(), gen())` has unspecified
argument evaluation order, so restructuring the draw would have silently
changed the RNG sequence. **With the flag off the output is byte-identical
to the previous HEAD** -- verified on 5 configurations (default skeleton
path on sum/gcd, composite on substring, second-array + crossover +
expr-mutation on sorting, loop-bias 0.0 on single_number): identical
solutions JSON and identical logs modulo timing lines. Phase 0 counters
(`ALGOSEARCH_DEBUG_PRUNE_COUNTS`) run in both modes and also tally
phase-2 candidates (const-op-const, `x - x`, `x + 0`/`x * 1`, `x * 0`)
without touching them. 2 new tests, `make test` 94/94.

**Phase 0 -- how much junk the unpruned grammar draws** (pooled off-arm
counts, suite B shown; suite A within ~1 point on every rule):

| shape | rate | of |
|---|---|---|
| `v = v` | 8.1% | scalar assignments |
| constant if/while condition | 5.8% | conditions |
| `swap(e, e)` | 3.5% | swaps |
| `arr[e] = arr[e]` | 0.5% | array assignments |
| always-erroring literal operand | 0.25% | arithmetic binops |
| *(info)* both operands literal | 9.2% | binops |
| *(info)* identity operand (`x + 0`, `x * 1`, ...) | 9.2% | binops |
| *(info)* self-cancel (`x - x`, `x ^ x`) | 2.7% | binops |
| *(info)* absorbing operand (`x * 0`, `x & 0`) | 0.5% | binops |

The 8-redraw bound was never exhausted (`redraws_exhausted = 0` in every
on-arm run).

**Phase 1 -- matched-seed A/B** (`scripts/run_prune_generation_ab.sh`,
summary via `scripts/summarize_prune_ab.py`; off/on arms of each seed run
concurrently with `--threads 4` each so machine load hits both equally).
Prediction registered in the script header before running: neutral on
success, equal or lower wall time.

- Suite A (default loop-bias 0.4, the modest battery's 17 problems and
  budgets, seeds 1000-1004): identical success on 16/17. The one
  difference, `reverse` 3/5 vs 4/5, was re-run on 25 fresh seeds
  (1005-1029): **25/25 vs 25/25** -- the original gap was noise (seeds
  1000-1004 are just harder ones). Pooled `reverse`: 28/30 vs 29/30.
- Suite B (isolation, loop-bias 0.0: sum/min/max/count/contains/sorting/
  gcd/single_number, plus section 32's substring/dominated_window with
  composite 1.0): identical success everywhere (0/5 on the first seven,
  1/5 vs 1/5 single_number, 5/5 vs 5/5 on the composite pair), mean
  n_reached within 0.2.
- Wall time: no effect. Per-problem means within noise; the on-arm was
  faster in ~60% of pairs overall and 13/25 on the `reverse` confirmation
  (8.9s vs 10.2s mean).

**Conclusion: the trivia are real (roughly 1 in 12 assignments, 1 in 17
conditions) but they are not what limits the search.** Redrawing them is
safe and neutral, exactly as predicted, so `pruneGeneration` stays opt-in
rather than becoming a default nobody can distinguish from the old
behavior. One honest limit of this A/B: neither suite has much of a
"boundary zone" at this budget -- suite A saturates in milliseconds via
skeletons, suite B floors at 0/5 because every isolation run exhausts
`--max-program-size 150` in ~20s (no run hit the wall-clock timeout) --
so a small effect on marginal problems cannot be ruled out, only a large
one. The documented bloat problem (section 29's 1600-node champions) is
junk *accumulated inside surviving programs* via growth, which this phase
does not touch by construction; that is phase 3's target, and section 29's
sorting stall is its natural test.

Data: `data/prune_generation_ab_20260927/`,
`data/prune_generation_ab_reverse_20260927/` (per-run logs, solutions JSON,
`err_*.txt` prune counters, `times.tsv`). Code: `src/search/
ProgramGenerator.{hpp,cpp}` (`prunePart`, `genPlain*`, `genCondExpr`,
counters), `src/ast/Expr.{hpp,cpp}`, `src/main.cpp`, `tests/
generator_tests.cpp` (`prune_generation_*`).

## 34. Phase-3 go/no-go check: the bloat in a stalled sorting run is mostly live code, not dead weight -- phase 3 dropped

Direct follow-up to section 33's closing question: is the stall documented
in section 29 (a champion program growing to 1600+ nodes while score stays
flat) removable junk, or code the search is actually relying on? Before
building phase 3 (materializing `canonicalText`'s fold+DCE pass onto beam
programs, not just its dedup hash), this was checked directly rather than
assumed.

**Instrumentation** (`ast/Stmt.{hpp,cpp}`'s `rawAndSimplifiedSize`,
`BeamSearch.cpp`'s `ALGOSEARCH_DEBUG_BLOAT`): reuses the exact same
fold-constant-conditions + backward-liveness-DCE pass `canonicalText`
already trusts for dedup, but returns `{raw node count, node count after
that pass}` instead of building a string -- cheap enough to call every 20
size-steps without materially slowing the search. Verified on a short smoke
run first: score/size/dead-fraction all stable and non-pathological (~22%),
confirming the instrumentation measures what it claims to.

**Decision rule, registered before running:** dead fraction at a stall
>50% -> phase 3 has a real target, build it. <20% -> the bloat is live,
drop the pruning line here. In between -> discuss with the data in hand.

**Run:** exact replay of section 29's configuration
(`scripts/run_sorting_20h.sh`'s parameters, seed 42, same
`--post-solve-sizes 150 --composite-bias 0.15` etc.), with
`ALGOSEARCH_DEBUG_BLOAT=1`, watched via a polling script rather than left
unattended for 20h. Two stalls were caught directly, not merely inferred:

- **N=39, score plateaued at 47/48 for 45+ minutes**, size flat at 251
  nodes (candidate pool identical between checks -- growth had genuinely
  stopped, not just the score). `dead_frac` steady at 13.5-14.3%.
- **N=40, score plateaued at 3/48 for 45+ minutes** (the low-score,
  churning-size pattern section 29 actually described as the pathological
  case) -- raw size oscillated 550-961 nodes without converging, but
  `dead_frac` stayed at 9.5-11.9% throughout.

Across all 292 instrumented samples in the run (levels 1-40), **the
maximum dead fraction observed was ~10%** (0.0999) -- it never
approached even the lenient 20% threshold, let alone 50%, at any point,
stall or not.

**Conclusion: phase 3 is dropped, a genuine negative result, not merely
deferred.** The bloat this project has been calling "growth corruption"
since section 27 is predominantly LIVE code by this file's own existing,
tested definition of dead (the same fold+DCE `canonicalText` already
applies) -- materializing that pass onto beam programs would shave ~10%
off a stalled candidate's size, not fix the stall. This also sharpens
what "growth corruption" actually is: not accumulated dead weight from
repeated divergence-window padding (section 29's own text speculated this
without checking), but the search churning through many different
~600-900-node variations that are each mostly doing SOMETHING, just not
converging on a correct one -- consistent with section 27's earlier,
harder-won finding that superficially similar bloat there was compensating
for a missing catalog piece, not incidental noise. A future attempt at
this problem needs a different lever (e.g. a size-based fitness penalty
that discourages the churn itself, or the periodic re-seeding section 29
also floated), not a simplification pass.

Kept: the diagnostic itself (`rawAndSimplifiedSize`,
`ALGOSEARCH_DEBUG_BLOAT`) -- cheap, off by default, no behavior change,
useful for the next time this question comes up on a different problem.
Not pursued further: phases 2/3/4/5 of section 33's five-phase plan (2 and
4 were never gated on this check and remain open if revisited later, but
neither was the priority item here).

Data: `data/bloat_diagnostic_20260928/` (`log.txt` the ordinary search
log, `bloat.log` the raw/simplified/dead_frac samples). `make test` 94/94
(instrumentation is diagnostic-only, no new test needed beyond the
existing suite already exercising `canonicalText`'s fold+DCE pass).

## 35. Behavioral redundancy of generation: 70-98% of scored candidates behave exactly like something already seen

First measurement for the "increase generation diversity" line opened after
section 34's general review. That review's synthesis: every real gain in this
log came from changing WHAT is generated (sections 5, 17, 27, 32), and every
attempt to widen what the BEAM keeps alive (sections 5, 21, 26, 28, 29) raised
distinct-program counts without creating a new shape. The obvious missing
number was how behaviorally diverse generation actually is -- this section
measures it before building anything (idea E: generation-time behavioral
dedup; also the baseline for ideas B/C/D).

**Instrumentation** (`ALGOSEARCH_DEBUG_BEHAVIOR`, `BeamSearch.cpp`, off by
default, no RNG use; byte-identical output when off, re-checked on two
configurations; `make test` 94/94). Each scored candidate gets a hash of its
exact behavior: (status, full output array) on every instance of the level,
with TIMEOUT/RUNTIME_ERROR hashed by status only. Per size step, a candidate
is a **duplicate** if its behavior equals a current beam survivor's or an
earlier candidate's in the same step -- i.e. its evaluation told the search
nothing new. Also recorded: a cheap 3-instance prefix hash (the pre-filter a
real dedup would need, since the full hash only exists after full
evaluation), its **false-merge** rate (prefix-duplicate but actually new
behavior), and **neutral** mutants (grownPool is interleaved [elite clone,
mutant], so a mutant identical in behavior to its own parent is detectable).

**Run** (`data/behavior_dup_measure_20260929/run.sh`, 2 seeds each):
isolation arm (loop-bias 0.0) sum/min/sorting/gcd; default arm
reverse/factorial; section 32's composite substring; min with
`--crossover-rate 0.2`. Pooled per config and candidate origin:

| config | origin | candidates | duplicate | of dups, score 0 | 3-prefix dup | false merge | neutral mutants |
|---|---|---|---|---|---|---|---|
| iso sum | fresh | 447k | 90.5% | 90.8% | 97.2% | 6.9% | |
| iso sum | mutant | 148k | 85.8% | 25.1% | 93.9% | 8.6% | 47.0% |
| iso min | fresh | 447k | 86.2% | 84.1% | 96.9% | 11.1% | |
| iso min | mutant | 148k | 83.0% | 19.6% | 94.0% | 11.8% | 40.2% |
| iso sorting | fresh | 447k | 91.7% | 85.1% | 97.5% | 6.0% | |
| iso sorting | mutant | 148k | 84.0% | 23.2% | 93.6% | 10.2% | 37.3% |
| iso gcd | fresh | 447k | 95.6% | 86.7% | 99.0% | 3.4% | |
| iso gcd | mutant | 148k | 93.5% | 76.2% | 97.0% | 3.6% | 19.7% |
| def reverse | fresh | 249k | 68.8% | 80.4% | 94.2% | 27.0% | |
| def reverse | mutant | 82k | 73.1% | 27.5% | 89.3% | 18.2% | 43.0% |
| def factorial | fresh | 447k | 97.0% | 88.0% | 98.9% | 2.0% | |
| def factorial | mutant | 148k | 87.4% | 47.6% | 94.7% | 7.7% | 12.7% |
| composite substring | fresh | 4.8k | 98.1% | 55.4% | 98.7% | 0.7% | |
| composite substring | mutant | 2.0k | 90.0% | 45.5% | 96.1% | 6.3% | 47.1% |
| min + crossover | crossover | 89k | 92.1% | 0.4% | 98.1% | 6.2% | |

**What this says:**

1. **Behavioral diversity of generation is tiny.** Only ~2-30% of scored
   candidates (typically ~10%) produce a behavior the search hasn't already
   seen in that step or in its beam. Textual dedup (`Dedup.hpp`) already
   ran on the fresh pool, so these are textually distinct programs that do
   the same thing.
2. **Fresh random programs collapse onto a few junk behaviors.** 80-90% of
   fresh duplicates score 0; the ordinary search log explains most of it:
   in `iso_min`, ~68% of all instance executions end in RUNTIME_ERROR
   (~16.9k errors of ~25k executions per size step). Random generation
   mostly produces programs that crash the same way.
3. **Mutants are 12-47% neutral**: roughly a third of all beam mutations
   leave behavior exactly unchanged. That is a free neutral network the
   search already walks (relevant to idea D: semantics-preserving rewrites
   would add to an already-large neutral fraction, not create one).
4. **Crossover is 92% duplicate but almost never junk** (0.4% of its dups
   score 0): transplants mostly land in places that don't change what the
   recipient computes.
5. **A 3-instance prefix is not a safe dedup filter**: false merges run 1-27%
   (median ~7%; `reverse` 18-27%) -- it would silently discard genuinely new
   behaviors, especially on problems whose early instances are uninformative.

**Honest limits.** Counts are per candidate, not cost-weighted: RUNTIME_ERROR
executions abort early and are cheap, so the compute a dedup could reclaim is
well below the ~90% duplicate rate (the expensive case is TIMEOUT, ~3% of
executions in `iso_min`). Duplicates are counted within one size step only;
across steps the rate can only be higher. Two seeds per config -- enough for
rates this far from 50%, not for fine comparisons between configs.

**Decision on idea E:** as a compute-saving dedup it is weak (cheap crashes
dominate the duplicates, and the only cheap pre-filter mis-merges up to 27%
of new behaviors). Not built. **The measurement itself is the useful
product**: "fraction of generated candidates with a new behavior" is a cheap,
problem-agnostic proxy for generation diversity, measurable in minutes, and
directly comparable across grammar changes -- the missing baseline for ideas
B (combinator algebra) and C (role-typed variables). A grammar change that
cannot raise it is unlikely to create new shapes; one that raises it is worth
the expensive generalization A/B. Point 2 also suggests a concrete first
target for C: most fresh junk is crashing programs, plausibly incoherent
index use (`arr[x]` with `x` an accumulator), which role-typing would prevent
by construction.

Data: `data/behavior_dup_measure_20260929/` (`err_*.txt` hold the
`[behavior-debug]` lines). Code: `src/search/BeamSearch.cpp`
(`behaviorHashFull`/`behaviorHashPrefix`, `BehaviorTally`).

## 36. Role-typed variables (idea C): new-behavior rate up 1.4-2.5x in the free grammar, crashes down; first gate passed, outcome not yet tested

Section 35 measured that ~90% of scored candidates duplicate an
already-seen behavior, mostly identical crashes, and proposed the
new-behavior rate as the cheap gate a grammar change must pass before an
expensive generalization A/B. This is the first idea put through that gate.

**Implementation** (`GenConfig::roleTypedVars`, `--role-typed-vars`, opt-in,
byte-identical when off -- re-checked on the same 3 configurations as
section 33; new test `role_typed_vars_keep_indices_and_values_apart`,
`make test` 95/95). Generic grammar only; skeletons and the composite family
keep their own bindings. Roles are fixed by variable index so they survive
later mutation: v0 is the read-only bound n, odd scratch variables are
indices, even ones are values. Array subscripts (reads, array assignments,
swaps) come only from index expressions -- index var, literal, index +/-
literal, n - literal. Index vars are only assigned a start (0/1), a step
(+/-1), n, or another index expression. Conditions compare index vs.
bound/index or value vs. value, never across roles. Pruning (section 33)
still applies to the typed draws.

**Measurement** (`data/role_typed_measure_20260929/run.sh`: exactly section
35's 8 configs and seeds with `--role-typed-vars` added; control = section
35's data, same binary behavior when off; comparison via
`scripts/compare_behavior_novelty.py`):

| config | new behavior, fresh | new behavior, mutant | runtime errors | timeouts | outcome (2 seeds) |
|---|---|---|---|---|---|
| iso sum | 9.5% -> **23.5%** | 14.2% -> 18.5% | 55.2% -> 36.9% | 3.4% -> 11.0% | N 2,1 -> 3,3 |
| iso min | 13.8% -> **19.4%** | 17.0% -> 19.3% | 49.7% -> 41.0% | 6.1% -> 11.6% | N 1,4 -> 3,4 |
| iso sorting | 8.3% -> **14.0%** | 16.0% -> 22.2% | 55.6% -> 44.4% | 3.5% -> 8.5% | N 1,3 -> 2,3 |
| iso min + crossover | 13.0% -> **24.3%** | 22.7% -> 38.3% | 49.8% -> 36.4% | 4.1% -> 11.4% | N 3,2 -> 3,5 |
| iso gcd | 4.4% -> 3.7% | 6.5% -> 6.0% | 65.4% -> 57.9% | 2.5% -> 5.3% | no records either arm |
| def factorial | 3.0% -> 2.8% | 12.6% -> 11.4% | 59.1% -> 56.2% | 4.3% -> 5.7% | SUCCESS both |
| def reverse | 31.2% -> **16.1%** | 26.9% -> 16.3% | 44.4% -> 19.1% | 7.7% -> 7.6% | S,- -> S,S |
| composite substring | 1.9% -> 1.9% | 10.0% -> 10.2% | 39.0% -> 38.8% | 1.0% -> 0.9% | SUCCESS both |

(new behavior = 1 - duplicate rate; errors/timeouts = share of all instance
executions; outcome = max n_reached per seed, no isolation run reaches
SUCCESS in either arm.)

**Reading:**

1. **The gate is passed where the idea acts.** On the four array problems in
   the isolation arm (free grammar only), fresh candidates produce a new
   behavior 1.4-2.5x more often, and runtime errors fall 8-18 points --
   the suspected mechanism from section 35 (incoherent subscripts crashing
   the same way) is confirmed as a real, large source of redundancy.
2. **Timeouts rise 2-3x in the same configs.** Coherent index loops no
   longer crash, so more of them run until the step limit (`while (v1 <
   v0)` without an increment). The redundancy moved partly from cheap
   crashes to expensive timeouts; any outcome A/B has to watch wall time.
3. **No effect where the free grammar isn't doing the work** (gcd is a
   magnitude problem with a 1-2 element array; factorial/substring are
   solved by a skeleton/composite), as expected.
4. **[Corrected in section 37: this reading was wrong.]** A real expressiveness cost, found by the metric itself: `reverse`'s
   new-behavior rate HALVED. Its natural subscript is `n - 1 - i`, which the
   index grammar cannot build (`n - literal` and `index +/- literal` only).
   Both seeds still solved via the skeleton, but in the free grammar this
   shape is now unreachable. Fix before any outcome A/B: add `n - index -
   literal` (and `index + index`) to `genIndexExpr`.
5. Outcomes moved slightly in C's favor (isolation max level up in 5 of 8
   seed pairs, down in none) but 2 seeds on a 0-SUCCESS floor is not
   evidence of anything yet.

**Status:** opt-in, default off. The cheap gate says C is worth an
expensive test; it does not say C helps. Next, in order: close the
subscript gap from point 4 and re-measure `reverse`; then a matched
10-seed isolation A/B on the outcome (max confirmed level, SUCCESS, wall
time) -- the first time since section 5 that an isolation-arm change has a
measured mechanism behind it.

Data: `data/role_typed_measure_20260929/`. Code: `src/search/
ProgramGenerator.{hpp,cpp}` (`genIndexExpr`, `genIndexUpdateExpr`,
`genTypedValueExpr`, `genTypedCondExpr`, `genStmtCondExpr`, typed branches
of `genPlain*`), `src/main.cpp`, `tests/generator_tests.cpp`,
`scripts/compare_behavior_novelty.py`.

## 37. Vacuous SUCCESS at max_level, a metric correction, and the role-typed outcome A/B: no genuine gain

Three things, in the order they were found while running section 36's
follow-ups.

### Correction to section 36: the `reverse` novelty "drop" was crash-pattern variety

Section 36 read `reverse`'s halved new-behavior rate as an expressiveness
gap (no `n - 1 - i` subscript) and planned to close it. Adding `n - index -
{0,1}` and `index + index` to `genIndexExpr` changed nothing (16.1% ->
16.4%), falsifying that reading. The actual cause is in the metric: the
behavior hash records TIMEOUT/RUNTIME_ERROR per instance by status, so a
program that crashes on SOME instances produces a "new behavior" for every
distinct crash pattern. Added a **clean new behavior** tally (new behavior
with no TIMEOUT/RUNTIME_ERROR on any instance; `new_clean` in
`[behavior-debug]`) and re-ran both arms on section 35's exact configs/seeds
(the control re-run reproduced section 35's numbers exactly):

| config | clean new behavior, fresh (off -> on) | raw new behavior, fresh |
|---|---|---|
| iso sum | 0.6% -> 3.2% | 9.5% -> 17.3% |
| iso min | 0.8% -> 3.6% | 13.8% -> 19.3% |
| iso sorting | 0.6% -> 2.4% | 8.3% -> 13.1% |
| iso min + crossover | 0.8% -> 3.9% | 13.0% -> 20.3% |
| iso gcd | 0.4% -> 1.0% | 4.4% -> 4.0% |
| def reverse | **4.8% -> 6.1%** | 31.2% -> 16.4% |
| def factorial / comp substring | unchanged | unchanged |

Under the unpruned grammar **fewer than 1% of fresh candidates produce a
new, non-crashing behavior**; role typing raises that 4-5x, and `reverse`'s
clean rate goes UP. The clean rate is the metric to use from here on
(`scripts/compare_behavior_novelty.py` prints both). The extra subscript
forms are kept (they are legitimate index shapes). Data:
`data/behavior_clean_control_20260929/`, `data/role_typed_v2_measure_20260929/`.

### A measurement defect: SUCCESS found at max_level is vacuous

The role-typed outcome A/B (`scripts/run_flag_ab.sh`, new generic
matched-seed isolation A/B; 7 array problems x 10 fresh seeds 2000-2009,
loop-bias 0.0, section 33 suite-B budget) first showed `reverse` **0/10 ->
6/10 SUCCESS**, faster in 10/10 pairs. `--benchmark` (10 seeds) rejected
**all 6** programs: every one fails at n=1 with a runtime error. All 6 had
`first_level = 8 = max_level`, `confirmed_levels = 1`.

Cause (`BeamSearch.cpp`): `requiredLevel = min(currentLevel +
invarianceLookahead, maxLevel)`. A program first solving the LAST level
gets `requiredLevel == currentLevel`, so the structural-invariance check is
satisfied without checking anything -- any program that memorizes the
last level's instances is labeled SUCCESS. Role typing makes the search
reach N=8 more often (mean N reached 5.6 -> 7.6), where the label is free.
`--benchmark` re-verifies independently and was never affected; any
analysis reading `final_status` straight from a solutions JSON was.
`scripts/summarize_prune_ab.py` now counts a SUCCESS only if first found
below `max_level`, and reports vacuous ones separately.

**Audit of this session's A/Bs with the corrected count:**
- Section 33, suite A `graph_reachability`: the corrected count was 1/5 vs
  **5/5** (4 of the off arm's "SUCCESS" were vacuous) -- an apparent
  pruning win. Not trusted on the discovery seeds: 30 fresh seeds gave
  6/30 vs 11/30 (9 vs 4 discordant pairs, McNemar exact p = 0.27), then
  100 more fresh seeds gave **24/100 vs 30/100 (20 vs 14, p = 0.39)**. No
  effect; the 5-seed split was luck. Section 33's "neutral" stands.
- Section 33, suite B `single_number`: corrected 1/5 vs 0/5 (the on arm's
  success was vacuous). Too few to mean anything; neutral stands.
- Section 36's role-typed A/B: see below.
- Earlier sections that counted SUCCESS from JSON without `--benchmark`
  (e.g. section 28's "SUCCESS levels" union) may include vacuous entries;
  not re-audited here. Negative results (0/N) cannot be inflated by this.

**Engine fix not applied** (it changes the meaning of a label every
existing analysis and archive uses): the natural fix is to label a
cascade that could not look ahead at all (`currentLevel == maxLevel`) as
UNCONFIRMED. Left as an explicit decision.

### Role-typed variables, outcome A/B: no genuine success, climbs further, costs time

`data/role_typed_ab_20260929/` (on arm with the v2 subscripts),
corrected count:

| problem | genuine SUCCESS off/on | mean N reached off/on | wall s off/on |
|---|---|---|---|
| sum | 0/10 / 0/10 | 1.9 / 2.8 | 18.5 / 25.5 |
| min | 0/10 / 0/10 | 2.3 / 3.7 | 22.2 / 28.4 |
| max | 0/10 / 0/10 | 2.2 / 2.7 | 19.2 / 21.5 |
| count | 0/10 / 0/10 | 1.0 / 1.0 | 22.8 / 24.8 |
| contains | 0/10 / 0/10 | 1.0 / 1.0 | 25.0 / 28.1 |
| sorting | 0/10 / 0/10 | 1.7 / 2.0 | 18.1 / 19.2 |
| reverse | 0/10 / 0/10 (0/6 vacuous) | 5.6 / 7.6 | 137.9 / 75.1 |

**Conclusion: idea C passes the diversity gate (4-5x clean novelty) but
does not produce a single generalizing program in the isolation arm at
this budget.** It makes the search fit more levels (N reached up on 5 of 7
problems) at 10-40% more wall time where it doesn't finish early -- more
coherent programs, more of them memorizing a level, none of them the
algorithm. This is the same shape as section 5's diversity result one
level down: more structurally valid variety, no new algorithm. It also
calibrates the section 35 gate: raising clean novelty is necessary-looking
but demonstrably not sufficient. `roleTypedVars` stays opt-in.

What this does NOT rule out: C as a base for B (the composite combinator
algebra), where the missing ingredient is coordinated structure, not
coherent subscripts -- section 7's lesson (the atomic skeleton, not any
single ingredient) suggests exactly this outcome for C alone.

Data: `data/role_typed_ab_20260929/`, `data/prune_generation_ab_graph_20260929/`,
`data/prune_generation_ab_graph100_20260929/`. Code:
`src/search/BeamSearch.cpp` (`behaviorAnyFailure`, `new_clean`),
`src/search/ProgramGenerator.cpp` (`genIndexExpr` v2),
`scripts/run_flag_ab.sh`, `scripts/summarize_prune_ab.py` (vacuous
split), `scripts/compare_behavior_novelty.py` (clean column).

### Follow-up: engine fix applied

`BeamSearch.cpp` now labels a cascade whose first solve is AT `maxLevel`
as UNCONFIRMED ("no lookahead possible"), since nothing beyond it can
confirm the program. Only the label and log line change -- search
progression never depended on it -- and `--benchmark` simply stops being
handed those candidates to reject. New test
`first_solve_at_max_level_is_unconfirmed_not_vacuously_success`,
`make test` 96/96. A cascade first solving at `maxLevel - 1` still counts
with one level of lookahead (weaker than the default 3, but not vacuous).

## 38. Combinator algebra (idea B): 9 problems solved in isolation from a product of catalogued axes, one algorithm no skeleton contains -- and a second verification weakness

**Why B, and what it is.** Every positive result in this log came from
generating coordinated structure (sections 5/7's atomic skeleton, 27/32's
composite with a shared binding context); every attempt to widen what the
beam keeps alive failed (sections 5, 21, 26, 28, 29), and single coherent
pieces alone (section 37's role typing) raised novelty without producing
algorithms. B generalizes the one mechanism that composed without a named
skeleton: `genCombinatorBlock` (`GenConfig::combinatorBias`,
`--combinator-bias`, opt-in) draws a block as a PRODUCT of independent
axes, all bound to one six-variable context:

- **layout** (the four input encodings this repo's problems use): whole
  array; key in arr[0]; length-prefixed; two length-prefixed sequences;
- **loop**: single counting loop; nested full; nested triangular (j from
  i+1); two converging pointers;
- **relation**: sum, running best, count-if, exists, scoped forall (section
  32's piece), xor, conditional swap, swap -- with a drawn comparator and
  write-back to arr[0] (none for in-place relations).

Every axis value is a primitive already catalogued in section 24.
Deliberately absent: pointer chasing (`cycle`) and "max over
already-computed table cells" (`lis`), the two held-out shapes the plenary
ruled must not be added ad hoc. Like the skeletons after section 30, the
block only fires in a context that runs exactly once (not inside if/while),
and -- with the flag on only -- a fresh program stops growing after it, the
same sibling-clobbering fix section 30 applied to `genBlockStmts`, now also
at the program root. Byte-identical when off (re-checked). `make test`
98/98, including `combinator_family_reaches_catalogued_array_problems`: a
phase-1 reachability test (sections 18/31's protocol) confirming the family
CONTAINS a program solving levels 2-6 for sum, min, max, contains, count,
single_number, reverse, sorting and substring.

**Criteria registered before running:** (1) gate -- clean new-behavior rate
above the existing composite's; (2) composition -- `--benchmark`-verified
solutions, isolation arm, on problems that score 0 there today; (3) the
interesting part -- correct algorithms that are NOT the text of any
existing skeleton.

**(1) Gate** (`data/combinator_gate_20260929/`, isolation arm, all three
arms at `--num-vars 8` so the binding context's variable count is not a
confound; 8 problems x 2 seeds):

| problem | clean new, fresh: control / composite 1.0 / combinator 0.5 | outcome control / composite / combinator |
|---|---|---|
| sum | 0.8% / 0.0% / 2.4% | 0/2 / 0/2 / 2/2 |
| min | 1.0% / 0.0% / 2.4% | 0/2 / 0/2 / 2/2 |
| max | 0.8% / 0.0% / 2.4% | 0/2 / 0/2 / 2/2 |
| count | 1.0% / 0.0% / 1.9% | 0/2 / 0/2 / 2/2 |
| contains | 0.9% / 0.0% / 1.2% | 0/2 / 0/2 / 2/2 |
| reverse | 1.9% / 0.0% / 3.3% | 0/2 / 0/2 / 2/2 |
| sorting | 0.8% / 0.0% / 2.8% | 0/2 / 0/2 / 2/2 |
| single_number | 2.2% / 0.0% / 3.1% | 0/2 / 0/2 / 2/2 |

The composite scores 0 clean novelty on these (it is specific to two-
sequence problems); the combinator raises it 1.3-3.5x over control. The
outcome column already answers (2) at this sample: 16/16 vs 0/16 vs 0/16.

**(2) Independent verification** (`data/combinator_benchmark_20260929/`,
`--benchmark --benchmark-seeds 5`, seed 3000, same isolation budget):

| problem | generalizing kept / rejected | champion |
|---|---|---|
| sum, min, max, count, contains, single_number | 5/0 each | canonical linear scan (accumulate / running best / count-if / exists / xor), O(n) |
| reverse | 5/0 | two converging pointers + swap, O(n) |
| substring | 5/0 | section 32's scoped-forall window, O(n*m) |
| sorting | 5/0 | see (3), O(n^2) |
| lis | 0/0 | -- (out of reach by construction, as designed) |
| cycle | **2/3** | see below -- a false positive |

45/45 on the nine target problems. The champions are clean, with none of
the dead code or index tricks free search normally carries: e.g. min is
`best = arr[0]; for i: if (arr[i] < best) best = arr[i]; arr[0] = best`.

**(3) An algorithm no skeleton in this project contains.** All five
sorting champions are
```
for i in 0..n: for j in 0..n: if (arr[i] <= arr[j]) swap(arr[i], arr[j])
```
-- the full (not triangular) double loop with the "wrong-looking"
comparison, known in the literature as "I can't believe it can sort"
(Fung, 2021). It is neither the SelectionSort nor the HeapSort/MergeSort
skeleton, nor bubble sort; it came out of the product (nested full x
conditional swap x `<=`). Verified exhaustively, not only by benchmark:
correct on all 21,728 arrays with values in [-2, 2] for n = 0..6 and [0, 2]
for n = 7, duplicates included.

Taxonomy (section 25's five columns) for the sorting result: solution found
-- yes; equivalent to a known algorithm -- yes (Fung's); only known
primitives -- yes; new STRUCTURE relative to this project's skeleton
library -- yes; new behavior -- no. For the other eight: "composition
learned" (a member of a designed family selected by search with no
problem-specific hint), not discovery -- the axes were designed with these
encodings in view.

**A second verification weakness, found by the `cycle` false positive.**
`--benchmark` accepted 2 `cycle` programs although the family has no
pointer-chasing piece. The fastest is "exists i<j with arr[i] == arr[j]"
(whole x triangular x exists(==)) preceded by `arr[arr[0]] = 0`. Checked
against EVERY functional graph for n = 1..6 (all heads, all next
arrays): **wrong on 33-47% of them** (e.g. head = n with every next = 0).
It passes only on the instance generator's own distribution, and
`--benchmark` draws its large-n instances from that same generator. So
`--benchmark` certifies "generalizes over the generator's distribution",
not "correct"; for problems with a small enumerable input space, an
exhaustive small-n check is a strictly stronger test (as done above for
sorting). Consequences: `cycle` stays unsolved (0 genuine); the nine
problems above were read by hand and sorting checked exhaustively; any
future claim on a held-out problem needs the exhaustive check, not the
benchmark alone. Not fixed in the engine yet (candidate: a
`Problem::exhaustiveInstances(n)` hook used by `--benchmark` when defined).

**What this does and does not show.** It shows the product-of-axes idea
works as a generation mechanism: one opt-in operator solves nine problems
in the isolation arm that no other mechanism in this log could (sections
5, 33, 37: 0/N), verified, with clean programs, and it found a correct
sorting algorithm that exists nowhere in the skeleton library. It does
NOT yet show discovery: the axes were written with these problems'
encodings in view, so eight of nine are "the family contains the answer
and search picked it". The honest next test is the one section 32 used:
new problems written AFTER the axes are frozen (this commit freezes them),
whose natural algorithms are axis combinations no existing problem uses
-- e.g. palindrome check (whole x converging x forall(!=)), inversion count
(whole x triangular x count-if(>)), has-duplicate (whole x triangular x
exists(==)), is-sorted -- plus `lis`/`cycle` as the out-of-reach ruler,
with exhaustive small-n verification from the start.

Data: `data/combinator_gate_20260929/`, `data/combinator_benchmark_20260929/`.
Code: `src/search/ProgramGenerator.{hpp,cpp}` (`genCombinatorBlock`,
trigger in `genStmt`, root stop in `generateRandomProgram`), `src/main.cpp`
(`--combinator-bias`, `resolveNumVars`), `tests/combinator_tests.cpp`.

## 39. Post-freeze test of the combinator algebra: 3 of 3 predicted-reachable new problems solved, 0 of 3 predicted-unreachable, and an exhaustive check in --benchmark

Section 38 left one honest objection standing: its axes were designed with
the solved problems' encodings in view, so eight of nine results were "the
family contains the answer and search picked it". This section runs the
test section 32 used for the scoped-conjunction piece: problems written
AFTER the axes were frozen (commit fb0e539), whose natural algorithms are
axis combinations no pre-existing problem used, with predictions registered
before any run.

### First, the verification fix section 38 called for

`Problem::exhaustiveInputs(n)` (opt-in hook, default none): every input of
size n over a small domain. `verifyGeneralization` (`--benchmark`) now
checks ALL of them for n = 1, 2, ... before its sampled large-n checks, with
correctness decided by `isSolution`. Implemented for `cycle` (every
functional graph with head, n <= 5, up to 46,656 inputs), `sorting` (every
array over [-1, 2], n <= 5) and the four new problems (every array over
[-1, 2], n <= 6). Regression tests: section 38's exact `cycle` false
positive is now rejected (`exhaustive_check_rejects_section_38_cycle_false_positive`)
and Fung's sort is still accepted. Problems without the hook are verified
exactly as before.

### The four new problems (`ScalarFromArrayProblem` base, whole-array input, answer in output[0])

- `palindrome` -- natural algorithm: whole x converging x forall(!=);
- `inversion_count` -- whole x triangular x count-if(>);
- `has_duplicate` -- whole x triangular x exists(==);
- `is_sorted` -- needs an adjacent comparison (arr[i-1] > arr[i]) or the
  negation of "an inversion exists"; the frozen axes have neither.

Instances mix exhaustive small-domain arrays, random arrays over [0, n],
constructed arrays for the rare label (mirrored halves, planted repeats,
sorted arrays with one adjacent swap) and the shared structural arrays;
majority label 57-75% for n >= 2. Tests per AGENTS.md: labels are ground
truth, both labels present, hand-written reference solutions pass every
level, adversarial instance and the exhaustive check, and a near-miss
palindrome (compares only the two ends) is rejected. `make test` 105/105.

**Predictions registered before running:** palindrome, inversion_count and
has_duplicate reachable; is_sorted not; cycle and lis stay out of reach.

### Results

Reachability (sampling 20,000 frozen-family blocks, correct on EVERY input
n = 2..6): palindrome 39, inversion_count 48, has_duplicate 33,
**is_sorted 0** -- exactly as predicted.

Search + `--benchmark` with the exhaustive check
(`data/post_freeze_benchmark_20260929/`; 5 seeds, seed 4000, `--num-vars
8`, budget as in section 38), three arms:

| problem | isolation (loop-bias 0.0) | skeleton library (loop-bias 0.4) | combinator 0.5 (isolation) |
|---|---|---|---|
| palindrome | 0 | 0 | **5 kept / 0 rejected** |
| inversion_count | 0 | 0 | **5 / 0** |
| has_duplicate | 0 | 0 | **5 / 0** |
| is_sorted | 0 | 0 | 0 / 2 (exhaustive, n <= 2) |
| cycle | 0 | 0 | 0 / **5** (exhaustive, n <= 2) |
| lis | 0 | 0 | 0 / 0 |

Every prediction held. The champions are the canonical algorithms, clean
(e.g. palindrome: `ok = 1; lo = 0; hi = n-1; while (lo < hi) { if (arr[lo]
!= arr[hi]) ok = 0; lo++; hi--; } arr[0] = ok`, O(n); inversion count and
has-duplicate: the triangular double loop, O(n^2)). The full skeleton
library solves none of the new problems, so this is not something the
project could already do. The exhaustive check caught all 7 false
positives in the combinator arm -- including the five `cycle` programs of
the same shape section 38's `--benchmark` had accepted.

### What this establishes, and what it doesn't

- The combinator algebra **generalizes beyond the problems it was designed
  around**: problems written after the freeze are solved iff their
  algorithm is a member of the product, and the reachability test predicts
  which ones in advance. That is the difference from every skeleton in
  this log -- a skeleton solves the problem it was written for; the
  product solves problems nobody wrote a block for.
- It does NOT discover anything outside the product. is_sorted is a
  trivially simple algorithm, yet unreachable because one axis value
  (adjacent comparison / negated exists) is missing; cycle and lis likewise.
  Capability is exactly the product's span, no more -- search selects, it
  does not invent.
- The natural long-term lever is therefore the axes themselves: each new
  axis value multiplies the span (e.g. an "adjacent pair" operand or a
  negated relation would reach is_sorted and any other adjacent-pair
  problem). Adding one is legitimate only under the same discipline:
  freeze, predict, test on problems written afterwards, verify
  exhaustively.
- Taxonomy (section 25): solution found -- yes (15/15); known algorithm --
  yes; only known primitives -- yes; new structure relative to the
  skeleton library -- yes for all three; new behavior -- no. "Composition
  learned", now demonstrated on problems unseen at design time.

Data: `data/post_freeze_benchmark_20260929/`. Code:
`src/problems/Problem.hpp` (`exhaustiveInputs`), `src/search/Benchmark.cpp`,
`src/problems/ArrayInstances.{hpp,cpp}` (`allArraysOver`,
`randomArraysOver`), `src/problems/ScalarFromArrayProblem.{hpp,cpp}`,
`src/problems/{Palindrome,InversionCount,HasDuplicate,IsSorted}Problem.{hpp,cpp}`,
`src/problems/{CycleDetection,Sorting}Problem.{hpp,cpp}` (exhaustive
inputs), `src/main.cpp`, `tests/exhaustive_verification_tests.cpp`,
`tests/post_freeze_problems_tests.cpp`.

## 40. Combinator v2: one new axis value (adjacent pairs) -- frozen, predictions registered BEFORE writing its test problems

Section 39 showed the combinator's capability is exactly its product's
span, so the lever is the axes. This adds ONE loop-axis value under the
same discipline: `Loop::Adjacent` (`--combinator-adjacent`, opt-in; without
it the v1 family of commit fb0e539 is byte-identical, verified on a
combinator run): a single pass `i` from start+1 with operands
`(elem(i-1), elem(i))`, combined with the existing pair relations
(count-if, exists, forall with one flat scope like the converging walk,
conditional swap, swap). No new relation, no negation.

**Frozen in this commit. Predictions, registered before any of the new
problems below exists:**
1. `is_sorted` (section 39, unreachable in v1) becomes reachable and
   solved: adjacent x forall (clear when elem(i-1) > elem(i)).
2. Three problems to be written after this commit, each predicted solved
   by v2 and unsolved by v1: `count_descents` (#i with arr[i-1] > arr[i];
   adjacent x count-if), `strictly_increasing` (adjacent x forall, clear
   on >=), `has_adjacent_equal` (adjacent x exists(==)).
3. Negative controls, predicted unsolved by v2 as well: `longest_run`
   (length of the longest run of equal adjacent values -- needs a counter
   that resets, not in the relation axis) and `max_adjacent_gap` (max of
   |arr[i] - arr[i-1]| -- needs "best of a computed difference", not in the
   relation axis). Plus `cycle`, `lis` unchanged.
4. Dilution: the 12 problems v1 solves stay solved under v2 at the same
   budget (each v1 member is drawn ~20% less often; expected cost is time,
   not success).

## 41. Idea A: filling the TSP pruning guard by exhaustive enumeration -- predictions registered before running

Section 18's four attempts to get free search to rediscover `cost < best`
inside the (hand-given) branch-and-bound shape all failed; its diagnosis was
that the guard sits at an isolated point of expression space with no
correct-and-selective neighbor, so mutation has no gradient to climb (every
non-pruning tautology scores identically). Enumeration needs no gradient.

**Mechanism** (`search/HoleFiller.{hpp,cpp}`, `--fill-tsp-guard` mode): the
branch-and-bound builder now takes the guard as a parameter
(`buildTspBranchAndBoundWithGuard`; the skeleton passes `cost < best` and
is byte-identical to before, checked against a HEAD build). Run the shape
once with guard = 1 (no pruning) on the training instances, recording the
11 scalars at every guard evaluation (new opt-in
`Interpreter::setConditionTrace`); enumerate every `term op atom` over those
11 variables and literals 0..3 (term = atom or atom +/- atom); collapse
candidates with identical truth vectors on the snapshots (observational
equivalence); run one representative per class on every training instance
(V = 2..6); keep those correct on all; rank by total interpreter steps;
check the winner on V = 7, 8, which selection never saw.

**Predictions (before running):**
1. The fastest correct class is `cost < best` or an equivalent (e.g.
   `best > cost`), or a strictly stronger sound bound -- with fewer steps
   than no pruning by roughly section 18's measured 2.6-8.6x.
2. The winner stays correct on held-out V = 7, 8 and keeps its speedup.
3. Framing, fixed now: the SHAPE is still hand-given (same status as
   section 18's scaffold test); success means the section 18 wall was a
   property of mutation-based search, not of the guard -- i.e. A is the
   right complement to B for "structure from the product, small holes by
   enumeration".

**Run 1 result (selection on V = 2..6) -- prediction 1 FAILED, informatively.**
76 training instances, 38,970 conditions -> 1,090 observational classes ->
96 correct on every training instance. Fastest: `(mBase - best) <= level`,
1.91x fewer steps than no pruning -- and WRONG on held-out V = 7 and 8. The
top of the ranking is unsound pruning that happens not to break at small V.
The reason is quantitative: at V <= 6 the sound guard `cost < best` saves
only 4% of steps (231,844 vs 241,216), so selection by speed on small
instances rewards aggressive unsound guards far more than the sound one.
Section 18's wall is therefore not only "no gradient for mutation": the
small training sizes barely separate sound from unsound pruning at all.

**Follow-up prediction, registered before run 2:** selecting on V = 2..8
(where unsound guards break more often and sound pruning pays more), the
fastest correct class is sound on held-out V = 9, 10 -- `cost < best` or an
equivalent/stronger sound bound.

**Run 2 result (selection on V = 2..8) -- follow-up prediction FAILED too.**
108 training instances, 1,322 classes, 120 correct on all of them. Fastest:
`pcBase <= best` (3.45x fewer steps than no pruning) -- WRONG on held-out
V = 9. Since pcBase = 1 + 3V, it prunes everything once a tour cheaper than
3V + 1 is known: an exploit of the instance generator's small weight range,
the same failure class as section 38's `cycle` false positive. The sound
`cost < best` is in the candidate set and correct, but ranks #55 of 120
(#57 of 96 in run 1): dozens of unsound, distribution-exploiting guards pass
the test filter and are faster.

**Conclusion for idea A as run here:** enumeration fixes REACH (the sound
guard is found without any gradient) but not SELECT. Correctness judged by
samples from the problem's own generator cannot separate sound pruning from
pruning that is only valid on that distribution, and ranking by speed then
actively prefers the unsound ones -- more instances from the same generator
do not fix this (run 2 had 108 instances and larger V). This refines
section 18: the wall is not only mutation's missing gradient but the
absence of a soundness signal. What would supply one without ML, both
using only the problem's own oracle:
- a per-evaluation soundness label: at each traced guard evaluation,
  "pruning here is safe" iff the best completion of the current partial
  tour (brute force, feasible at V <= 8) cannot beat `best`; require zero
  unsafe prunes, then rank by speed;
- adversarial instance families built against pruning (wide weight ranges,
  near-tie tours), so distribution-specific guards fail the filter.
Not done yet.

Data: `data/fill_tsp_guard_20260929/`.

## 40 (results). Combinator v2: every registered prediction held; plus search reaches beyond the v1 product occasionally

Reachability (20,000 sampled blocks, correct on EVERY input n = 2..6), v1 /
v2: is_sorted 0 / 40, count_descents 0 / 33, strictly_increasing 0 / 31,
has_adjacent_equal 0 / 23, longest_run 0 / 0, max_adjacent_gap 0 / 0.

Search + `--benchmark` with the exhaustive check
(`data/combinator_v2_benchmark_20260929/`, isolation arm, 5 seeds, seed
5000; v1 and v2 of each problem run concurrently):

| problem | v1 kept/rejected, wall | v2 kept/rejected, wall | prediction |
|---|---|---|---|
| is_sorted | 1/2, 141 s | **5/0, 2.1 s** | 1 held |
| count_descents | 0/0, 218 s | **5/0, 2.4 s** | 2 held |
| strictly_increasing | 0/1, 166 s | **5/0, 2.4 s** | 2 held |
| has_adjacent_equal | 2/1, 88 s | **5/0, 2.0 s** | 2 held |
| longest_run | 0/0 | 0/0 | 3 held (negative control) |
| max_adjacent_gap | 0/0 | 0/0 | 3 held (negative control) |
| 12 previously solved | 59/60 kept (sum: 4 distinct) | 60/60, same wall (+/- 0.3 s) | 4 held: no dilution |

**The surprise: v1 found 3 correct programs outside its product.**
Reachability says no single v1 block solves is_sorted or
has_adjacent_equal, yet v1's search produced verified ones (exhaustive
check passed):
- has_adjacent_equal: the triangular x exists(==) block with ONE point
  mutation -- the inner increment `j = j + 1` became `j = n`, so the inner
  loop runs once (j = i + 1): triangular turned into adjacent. Clean, O(n).
- is_sorted: 102 nodes, three chained blocks plus mutations (`v0 = 3` in
  place of an increment). Traced by hand: block 2 overwrites arr[0] with an
  element taking part in an inversion among positions 1..n-1 if there is
  one, block 3 flags any later element smaller than arr[0]. Correct, O(n^2),
  ugly.

**Corrected reading of section 39:** the product's span is the RELIABLE
capability (5/5 in about 2 s); beyond it, search composes blocks and
mutations into correct programs occasionally (1-2 of 5 seeds, 90-140 s,
often bloated). Adding the axis value turns an occasional, slow, ugly
success into a reliable, instant, clean one -- and costs nothing on the
problems already solved. `--combinator-adjacent` stays opt-in (v1
reproducibility); promoting it into the default product is the natural
next step.

Data: `data/combinator_v2_benchmark_20260929/`.

## 42. Adjacent-pair axis promoted into the default combinator family (branch `combinator-adjacent-default`) -- test prepared, not yet run

Section 40 found the adjacent-pair loop axis costs nothing on the 12
problems v1 already solved and makes four more reliable. It is now part of
the default combinator product (`GenConfig::combinatorAdjacent = true`).
`--combinator-v1` restores the frozen v1 family of commit fb0e539, checked
byte-identical against a pre-change build; the new default is
byte-identical to the old `--combinator-adjacent` (still accepted, now a
no-op), so section 40's v2 arms already are the validation of this
promotion. **Reproducibility note:** the v1 arms of the committed scripts
in `data/combinator_gate_20260929/`, `data/combinator_benchmark_20260929/`,
`data/post_freeze_benchmark_20260929/` and
`data/combinator_v2_benchmark_20260929/` predate this change; re-running
them now needs `--combinator-v1` added. Tests:
`combinator_default_family_reaches_adjacent_pair_problems`,
`combinator_v1_family_does_not_reach_is_sorted`; `make test` 112/112.

**Still open, prepared for the next joint test:** `combinatorBias` itself
stays opt-in. `scripts/run_combinator_default_battery.sh` A/Bs combinator
0.5 on top of the project's default skeleton library (loop-bias 0.4) over
the modest battery's 17 problems plus the 12 combinator-era problems, 5
matched seeds; `scripts/summarize_prune_ab.py` reads its output.
**Predictions, registered now:** no problem of the 17 loses genuine
successes (the skeleton path is unchanged, the combinator only adds
candidates); the combinator-era problems go from 0 (skeleton library alone,
as in section 39) to solved wherever they are in the v2 product, i.e.
everything except longest_run, max_adjacent_gap, cycle and lis. The one
risk worth watching: the combinator's early stop in
`generateRandomProgram` also stops after a SKELETON block when the
combinator is on, which could change how the 17 problems' fresh programs
look.

## 42 (results). Combinator on top of the skeleton library: every prediction held, no regression, two original problems improve

`data/combinator_default_battery_20260929/`
(`scripts/run_combinator_default_battery.sh`: loop-bias 0.4, num-vars 8,
seeds 6000-6004, combinator 0 vs 0.5 run concurrently per seed). Genuine
SUCCESS = first found below max_level (section 37); `cycle` additionally
re-verified with `--benchmark`'s exhaustive check.

| suite | problem | off | on | wall s off / on |
|---|---|---|---|---|
| A | 15 of the 17 original problems | 5/5 each | 5/5 each | unchanged |
| A | reverse | 2/5 | **5/5** | 153.5 / 0.2 |
| A | graph_reachability | 2/5 | 3/5 | 6.0 / 0.5 |
| C | substring, palindrome, inversion_count, has_duplicate, is_sorted, count_descents, strictly_increasing | 0/5 each | **5/5 each** | 17-50 / 0.2-0.4 |
| C | has_adjacent_equal | 2/5 | **5/5** | 34.9 / 0.4 |
| C | longest_run, max_adjacent_gap, lis | 0/5 | 0/5 | -- |
| C | cycle | 1/5 label | 2/5 label | all 3 REJECTED by the exhaustive check (n = 1-2): 0 genuine in both arms |

Predictions: no loss on any of the 17 -- held (two improve); everything in
the v2 product goes from 0-2/5 to 5/5 -- held; longest_run,
max_adjacent_gap, cycle, lis stay unsolved -- held (cycle only after the
exhaustive re-check; its JSON labels alone would have said otherwise, the
same lesson as sections 37-38). The flagged risk (the root stop after a
skeleton block changing the 17 problems) did not show up in any outcome.

**Conclusion:** turning the combinator on alongside the skeleton library
costs nothing measured and strictly widens what the default configuration
solves. Candidate next change: make `combinatorBias` part of the default
configuration (it is still opt-in; that switch would change every default
run's RNG stream, so it deserves its own commit and a full-battery check
like this one, which it now has).
## 43. TSP guard selection with per-evaluation soundness labels (branch `tsp-guard-soundness`) -- test prepared, not yet run

Section 41 found enumeration REACHES `cost < best` but selection (correct on
sampled training outputs, then fastest) picks unsound, generator-exploiting
guards. This supplies the missing soundness signal from the problem's own
oracle, no learning involved:

- `Interpreter::setConditionTrace` now also hands the array to the
  callback, so the branch-and-bound state (partial path, `used[]`) is
  visible at each guard evaluation.
- `tspPruneIsSafe(vars, arr)` (`search/HoleFiller.{hpp,cpp}`): pruning an
  evaluation is SAFE iff no tour completing the current partial path
  through candidate `c` costs strictly less than `best` -- exact brute force
  over the unvisited vertices (with a running-cost cutoff), the textbook
  admissibility condition.
- `--fill-tsp-guard --soundness-labels`: keeps every guard evaluation (not
  a 1-in-7 sample), labels each one, and rejects any candidate class that
  would prune at least one unsafe evaluation, before ranking the rest by
  steps.
- Tests: the oracle on a hand-built V = 3 state (unsafe when a strictly
  better tour lies below, safe on a tie or when nothing beats `best`), and
  the enumeration contains `cost < best`. `make test` 113/113. A V <= 3
  smoke run confirms the code path and that `cost < best` has 0 unsafe
  prunes; its numbers are too small to mean anything.

**Predictions, registered before the real run**
(`scripts/run_tsp_guard_soundness.sh`: seeds 42 and 7, selection on V = 2..6
and V = 2..8, plain vs. labeled):
1. With labels, the winner is correct on held-out V = maxN+1, maxN+2 in all
   four configurations (plain: wrong, as in section 41).
2. The labeled winner is `cost < best`, an equivalent, or a strictly
   stronger SOUND bound -- the natural candidate being `cost + 1 < best`
   (sound with positive integer weights, since at least the closing edge
   remains) -- and is at least as fast as `cost < best` on held-out V.
3. Caveat fixed in advance: labels are computed only on the traced
   training states, so "zero unsafe prunes" is still empirical, not a
   proof; a guard could be unsafe only on states the training instances
   never reach. The held-out check measures how much that matters.

## 43 (results). Soundness labels do NOT fix selection -- prediction 1 failed, for the reason caveat 3 anticipated

`data/tsp_guard_soundness_20260929/` (`scripts/run_tsp_guard_soundness.sh`,
30 s total):

| config | plain winner on held-out | labeled: classes rejected as unsafe | labeled winner on held-out |
|---|---|---|---|
| seed 42, select V <= 6 | wrong at V=7, 8 | 5 | SAME guard, wrong at V=7, 8 |
| seed 42, select V <= 8 | wrong at V=9 | 22 | SAME guard, wrong at V=9 |
| seed 7, select V <= 6 | wrong at V=7 | 7 | wrong at V=7, 8 |
| seed 7, select V <= 8 | wrong at V=9 | 5 | SAME guard, wrong at V=9 |

(The V=10 rows are uninformative: the hand guard and no-pruning runs hit
the 50M step limit there, so "hand ... 4319 steps" is an aborted run, not a
wrong answer.)

The oracle works as designed -- `cost < best` has 0 unsafe prunes in every
configuration, and 5-22 classes are rejected -- but **the winning unsound
guards have zero unsafe prunes on the training states too**. They compare
`best` against layout constants that grow with V (e.g. `(mBase - best) <=
level` with mBase = 1 + 4V): at the training sizes they happen to prune only
where it is safe, and the relationship breaks as V grows. Labels computed on
traced training states are an empirical check over the same distribution,
exactly caveat 3. The sound guard still ranks mid-pack (#40-#83).

**What this rules in:** the missing signal is invariance across SCALE, not
more labels at the same scale. The natural next test is metamorphic:
multiplying every weight by a constant k (or adding k to every edge, which
shifts every tour by V*k) preserves which pruning decisions are sound for a
cost-based guard, while guards that compare costs to layout constants or
small literals flip. Requiring a candidate to make the same decisions (or
keep zero unsafe prunes) on scaled copies of the training instances should
eliminate exactly the class that wins today. Not implemented yet.

## 44. Metamorphic weight scaling for TSP guard selection -- predictions registered before running

Section 43 showed the winning unsound guards have zero unsafe prunes at the
training weight scale: they compare `best` or `cost` with layout constants
(`mBase = 1 + 4V`, `pcBase = 1 + 3V`) or small literals, which happens to be
safe while tour costs sit in the same numeric range as those constants.
`--metamorphic` adds copies of every training instance with all edge
weights multiplied by 10 and by 100 (0 = no edge is preserved; k <= 100 keeps
tour costs below the skeleton's 100000 "infinity"). Scaling preserves every
tour's rank, so a sound cost-vs-cost guard such as `cost < best` behaves
identically at every scale; a guard tied to a fixed number either starts
pruning unsafely at some scale (rejected by the section 43 labels, which now
span all scales) or stops pruning (demoted, since ranking sums steps over
all scales). Held-out adds an unseen scale x7 at V = maxN+1, maxN+2. A sanity
line fires if `cost < best` or no pruning is ever incorrect on the scaled
set (checked clean at V <= 3).

Criterion fixed in advance: NOT "identical decisions
at every scale" -- that would reject legitimately stronger integer bounds
like `cost + 1 < best` -- but zero unsafe prunes at every scale plus speed
summed over scales.

**Predictions** (`METAMORPHIC=1 scripts/run_tsp_guard_soundness.sh`, seeds
42 and 7, selection V <= 6 and V <= 8):
1. With `--soundness-labels --metamorphic`, the winner is correct on
   held-out V = maxN+1, maxN+2 (original and x7 scale) in all four
   configurations.
2. The winner is `cost < best`, an equivalent (e.g. `best > cost`,
   `cost + 1 <= best`) or a stronger sound bound, and is at least as fast as
   `cost < best` on held-out.
3. Known way this can still fail, stated now: a guard whose unsoundness
   does not involve weights at all (e.g. comparing `level` with `size` or
   another V-dependent constant) is invisible to weight scaling; if one
   wins, the next missing invariant is scale in V, not in weights.

### Section 44 attempt 1 result: prediction 1 FAILED -- the aggregation, not the transform

`data/tsp_guard_metamorphic_20260929/meta_*.txt`: in 3 of 4 configurations
the winner is the same unsound guard as without scaling (e.g. `(mBase -
best) <= level`), wrong on held-out; 3-11 classes rejected as unsafe, `cost
< best` ranks #40-#76. The transform did what it was designed to do: at x10
and x100 these guards do not prune UNSAFELY -- they stop pruning at all
(`best` outgrows the constant, the guard is always true). The ranking undid
it: summing steps over scales lets a ~2x gain at scale 1 outweigh no-op
behavior at the other two, while the sound guard's gain is only ~4% at
every scale.

### Attempt 2, registered before running: rank by the WORST per-scale speedup

`--rank-worst-scale`: among candidates correct and with zero unsafe prunes
at every scale, rank by min over scales of (steps without pruning / steps
with the guard). A guard that helps only at one scale then ranks like no
pruning (worst ~1.0); a scale-invariant one keeps its speedup at every
scale. This rule was formed AFTER seeing attempt 1's data, so besides
re-running seeds 42 and 7 it is tested on generator seeds 11 and 23, which
have not been looked at. **Predictions:** in all 8 configurations (4 seeds x
select V <= 6, 8) the winner is correct on held-out V = maxN+1, maxN+2 at
scales x1 and x7, and is `cost < best`, an equivalent, or a stronger sound
bound. Known remaining failure mode: an unsound guard that never involves
weights (V-dependent only) -- weight scaling cannot see it.

### Attempt 2 result: `cost < best` rediscovered automatically when selecting on V <= 8 -- 4 of 4 seeds, including 2 unseen; fails at V <= 6

`data/tsp_guard_worstscale_20260929/worst_*.txt` (`--soundness-labels
--metamorphic --rank-worst-scale`):

| generator seed | select V <= 8: winner | held-out V=9 (x1, x7) | select V <= 6: winner | held-out V=7 / V=8 |
|---|---|---|---|---|
| 42 | `best > cost` | correct, same steps as hand | `(best - w) > cost` | wrong / wrong |
| 7 | `best > cost` | correct, same steps as hand | `(best - cost) != cost` | wrong / correct |
| 11 (unseen) | `best > cost` | correct, same steps as hand | `(best - cost) != cost` | correct / wrong |
| 23 (unseen) | `best > cost` | correct, same steps as hand | `(level + 3) != c` | wrong / wrong |

(V = 10 rows are uninformative: the hand guard and no pruning also hit the
50M step limit there -- "4319 steps" is an aborted run.)

`best > cost` IS `cost < best`. Selecting on V <= 8, the pipeline --
exhaustive enumeration, observational equivalence, per-evaluation soundness
labels, weight-scaled copies (x1, x10, x100), rank by worst per-scale
speedup -- **rediscovers section 18's hand-written guard in all four
generator seeds, two of them never inspected**, and it matches the hand
guard's steps exactly on held-out V = 9 at both scales. Selecting on V <= 6
still fails 4/4, though `cost < best` climbs from rank ~#60 to #2-#6: at
those sizes sound pruning buys only 1.6-4%, below the noise that
distribution-specific guards exploit.

**Prediction scorecard:** held for selection on V <= 8 (4/4, including the
unseen seeds); refuted for V <= 6 (0/4). **Conclusion:** section 18's wall
is not a property of the guard. Mutation could not reach it (no gradient);
enumeration reaches it; and it can be SELECTED from the problem's own
oracle, without learning, once three signals are combined -- soundness on
traced states, invariance under a rank-preserving transform (weight
scaling), and training instances large enough that sound pruning pays more
than noise. None of the three was sufficient alone (sections 41, 43, 44
attempt 1). The shape is still hand-given (same status as section 18's
scaffold): this establishes the "structure from the product, holes by
enumeration" complement to B, not discovery of branch-and-bound itself.

### Pre-merge check, registered before running

1. Robustness: 10 generator seeds never used before (101-110), selection on
   V <= 8, full pipeline (`--soundness-labels --metamorphic
   --rank-worst-scale`). Prediction: in at least 9/10 the winner is
   `cost < best`, an equivalent or a stronger sound bound, correct on
   held-out V = 9 at x1 and x7.
2. No harm to discovery: this branch adds a pointer comparison to every
   If/While evaluation in the interpreter (the condition trace). Prediction:
   ordinary search runs are byte-identical to main's and their wall time is
   within noise (<= 3% slower on a timed battery of 6 runs).

### Pre-merge check results: both predictions held

1. Robustness (`data/tsp_guard_premerge_20260930/`): on all 10 unseen
   generator seeds (101-110), selection on V <= 8 picks `best > cost`
   (= `cost < best`), correct on held-out V = 9 at x1 and x7, no sanity
   failure. Together with section 44: 14/14 generator seeds at V <= 8.
2. No harm to discovery: 6 ordinary search configurations (default
   skeleton path on sum/sorting/gcd, isolation reverse, composite substring,
   crossover + expression mutation on sorting), 3 alternating repetitions
   each, `--threads 1`: every solutions file byte-identical to main's; total
   wall +1.4% (64.32 s vs 65.24 s, dominated by the 64 s reverse run at
   +1.4%), within the registered 3%.

## 45. Sorting diversity is skeleton diversity: with the three sort skeletons off, no general sort is found

Question: how many genuinely different sorting algorithms does the search
find when given many seeds and every diversity lever? Four runs, same
diversity configuration throughout (`--diversity-rate 0.2
--shape-niche-rate 0.2 --behavior-niche-rate 0.15 --score-band-rate 0.15
--crossover-rate 0.3 --post-solve-sizes 15 --composite-bias 0.3`,
`--beam-width 500 --candidates 1500`). Distinct programs were classified by
text patterns (scripts not committed), not by behavior.

| run | data | config delta | result |
|---|---|---|---|
| A | `data/sorting_diversity_seeds_20261001/` | `--max-n 5 --num-vars 4 --max-program-size 150`, `--benchmark`, 300 seeds (10000-10299) | 1549 distinct texts, 482 after variable renaming, 239 shapes; 1607 kept / 115 rejected (all: runtime error at n=1). ONE algorithm: selection sort (+22 texts of an "exchange" variant, same skeleton) |
| B | `data/sorting_diversity_recursion_20261002/` | A + `--recursion-call-bias 0.1 --second-array-bias 0.05`, same seeds | 1948 texts / 699 renamed / 375 shapes; 2014 kept / 153 rejected. Still only selection sort: every `aux`/`call` occurrence is inert padding (`aux[v7] = 0;`, a dead `call` in an `else`) |
| C | `data/sorting_big_recursion_20261002/` | B with `--max-n 100 --num-vars 10 --max-program-size 3000 --step-limit 50000`, no `--benchmark`, 60 seeds (10000-10059) | 60/60 seeds reach N=100 in 110-155 s; final champion is merge sort in 60/60, selection appears in 60/60, heap sort in 5/60 |
| D | `data/sorting_noskeleton_20261002/` (`scripts/run_sorting_noskeleton.sh`) | C with `ALGOSEARCH_DISABLE_SKELETON=selectionsort,heapsort,mergesort`, 8 seeds (20000-20007) x 2 h wall, `--threads 2` | **0 SUCCESS records in 8/8 seeds.** Max level 3-23, all UNCONFIRMED |

**Every sort found in A-C is a hand-written skeleton.** Selection sort is
`genSelectionSortSkeletonBlock`, merge sort is `genMergeSortSkeletonBlock`
(copied verbatim, sometimes with junk prepended), heap sort is
`genHeapSortSkeletonBlock`. Merge sort only appears in C because its gate
needs `numVars >= 7` and `recursionCallBias > 0`. The rising distinct-program
counts from A to B to C are variable renaming and inert statements around
the same injected blocks, the same inflation section 29 reported.

**D, the actual discovery test, is a negative result.** The
`divideconquer` skeleton (split and recurse, no combine step) stayed
available. No seed produced a program that generalizes. Seeds that climbed
furthest (20005: N=23, 20002: N=20) did it by per-level memorization: the
N=22 champion of 20005 is ~500 nodes of repeated loops, counter increments
(`v9 = (v9 + 1);` dozens of times) and reassignments of `v0` that tune the
program to each N. Seed 20004 bloated to a 21486-node champion while stuck
at N=11 (section 29's growth corruption; `--max-program-size` does not
bound a single program's size).

**Near miss worth recording.** Inside that 20005 champion the search
assembled `if (arr[i] > arr[i+1]) { swap(arr[i], arr[i+1]); i = 0; } i = i + 1;`,
the core of gnome sort (adjacent swap, restart scan). It is subtly wrong:
after the reset the pair (0, 1) is never re-compared, and the `i < n` bound
reads `arr[n]`. Hand-checked on `[2, 3, 1]`: fails. Instead of fixing those two
edge cases, the search patches them per level with junk. `call(...)` does
appear in D (0 to ~2000 occurrences per seed log), but in the inspected
program only as a misplaced call with meaningless arguments, nothing
resembling a combine step.

**Conclusion.** Neither many seeds, every diversity lever, nor 16 CPU-hours
without skeletons yields a sorting algorithm the grammar was not handed.
**Scope correction (added after section 46):** all four runs had
`--combinator-bias` at its default 0.0. With the combinator on, the same
no-skeleton configuration solves sorting in 8/8 seeds (section 46), so
"no general sort is found" holds only for the skeleton/composite grammar
without the combinator.
That is section 5's finding (grammar prior >> population diversity)
restated for sorting with the skeletons explicitly removed. The gnome sort
near miss suggests the bottleneck is closing off-by-one edge cases, not
finding the idea. A repair or simplification step aimed at those (cf.
section 29's missing simplification pass) is the obvious next lever, not
more diversity.

Infrastructure change for D: `ALGOSEARCH_DISABLE_SKELETON` now accepts a
comma-separated list (`ProgramGenerator.cpp`); a single name behaves as
before. `make test` 115/115.

Caveats:
- A and B counts sum over 6 parallel `--benchmark` processes, so a text
  found in two processes is counted twice in kept/rejected; it is not known
  which texts the 115/153 rejections were.
- C and D used no `--benchmark` re-verification.
- In D, only the best program of seed 20005 was inspected in depth.

## 46. Sorting without sort skeletons, combinator on: 8/8 seeds solve, and bubble and gnome sort appear by composition

Follow-up to section 45, whose no-skeleton run D had `--combinator-bias`
at its default 0.0. Predictions were registered before running (in the uncommitted run
directory; restated verbatim in the scorecard below). Run script:
`scripts/run_sorting_noskeleton_combinator.sh`. Both arms used
`ALGOSEARCH_DISABLE_SKELETON=selectionsort,heapsort,mergesort
--combinator-bias 0.5` (v2 axes, adjacent pairs included) and section 45's
diversity flags:

- **D'**: section 45's run D unchanged (`--max-n 100 --num-vars 10`,
  recursion + aux) plus the combinator; seeds 20000-20007, 30 min wall
  each.
- **E**: `--benchmark` (exhaustive check for sorting, section 39) at
  `--max-n 6 --num-vars 8 --beam-width 500 --candidates 1500
  --max-program-size 150`; 200 seeds (30000-30199) in 4 processes.

Before running I read the generator: the `Adjacent` loop is ONE flat pass
(`elem(i-1)`, `elem(i)`), i.e. a single bubble pass, not bubble sort.

**Results:**

| | D (section 45, combinator off) | D' (combinator on) |
|---|---|---|
| seeds with a genuine SUCCESS (first solve below max-level) | 0/8 | **8/8** |
| max level reached in 30 min (D: 2 h) | 3-23 | 65-70 |
| distinct genuine programs per seed | 0 | 15-26 |

E: 1227 kept / 13 rejected (all 13: runtime error at n = 1), all
classified O(n^2) (exponent 1.76-2.22). Family classification by text
pattern over the union of D' and E (1143 distinct texts, 520 shapes after
erasing variable names and literals):

| family | texts | seeds (E of 200 / D' of 8) | origin |
|---|---|---|---|
| Fung's "I can't believe it can sort" (full nested loop x conditional swap) | 722 | 196 / 8 | one combinator draw (section 38) |
| exchange sort (triangular loop x conditional swap) | 289 | 92 / 8 | one combinator draw |
| unclassified | 127 | 52 / 6 | the inspected ones are Fung variants with junk or a reversed operand order |
| **gnome sort** (adjacent pass + index reset) | 4 | 2-3 / 0 (seed union not computed) | single-pass block + mutation adding `i = 0` after the swap |
| **bubble sort** (adjacent pass inside an outer loop) | 1 | 1 / 0 | single-pass block + an outer loop around it |

The gnome sort found is the clean, correct version of section 45's near
miss: `i = 1; while (i < n) { if (arr[i-1] > arr[i]) { swap; i = 0; } i = i + 1; }`.
The combinator's `elem(i-1)`/`elem(i)` indexing starting at `i = 1` is
exactly what fixes the two edge cases (pair (0, 1) never re-checked,
`arr[n]` read) that free search could not fix in section 45. The bubble
sort is the unoptimized `n` passes:
`while (k < n) { j = 1; while (j < n) { if (arr[j-1] >= arr[j]) swap; j = j + 1; } k = k + 1; }`.
I could not tell from the logs which texts the 13 rejections were. So
the bubble sort and both gnome variants (one resets via an uninitialized
`v1`, i.e. 0) were re-checked separately, by a Python transcription of the
printed programs: correct on all ~21.7k arrays with values in [-2, 2] for
n = 0..6 and [0, 2] for n = 7. None of them can fail at n = 1, where every
rejection happened, because the loop is not entered.

**Prediction scorecard:**
1. D' >= 7/8 genuine: **held** (8/8).
2. Every solution is a nested-loop x conditional-swap product:
   **refuted, informatively.** 973 of the 1143 texts are, but gnome sort and
   bubble sort are not single draws: each is the one-pass adjacent block
   plus a structural addition found by search, which is section 40's "search
   reaches beyond the product occasionally" again.
3. Bubble sort in at most 1 in 5 seeds of E: held (1/200; gnome 2-3/200).
4. Nothing sub-quadratic, no non-inert call/aux: held. All 1227 are
   O(n^2); the 2 D' programs that call `call(...)` do so with an empty range
   (`call(v0, j)`, `call(v0, 3)`), a no-op.

**Conclusion.** Section 45's negative result was about the grammar without
the combinator, not about sorting. With the combinator on and every sort
skeleton removed, the search finds four distinct correct quadratic
sorting algorithms, none of them a skeleton: Fung's and exchange sort
reliably (one draw each), and gnome sort and bubble sort rarely (a draw plus
a found structural step). What it does not find is anything sub-quadratic:
merge and heap sort remain skeleton-only (section 45's run C), and
section 45's divide-and-conquer skeleton still never acquires a combine
step. The gnome sort result also answers section 45's open question about
its near miss: the missing ingredient was the correctly offset adjacent
indexing, which the combinator axis supplies, not more search budget.

Caveats: family classification is by text pattern, with only the rare
families and the smallest unclassified texts read by hand. The ~21.7k-array
check of the rare programs used a Python transcription, not the
interpreter. D' used no `--benchmark` (its count uses section 37's
first-solve-below-max-level rule).

## 47. Combinator as the default, against the TRUE current default -- predictions registered before running

Section 42 compared combinator 0 vs 0.5 with both arms at `--num-vars 8`,
but the real default runs most problems with 4-6 variables, and the
combinator only fires at >= 8 (`resolveNumVars` raises it, even when
`--num-vars` is passed explicitly). So section 42 never measured what
flipping the default would actually change. Here the off arm is the
modest battery's exact command (its per-problem `--num-vars` in suite A;
nothing passed in suite C), and the on arm is the same command plus
`--combinator-bias 0.5`. Both arms use 15 fresh matched seeds (7000-7014),
`--beam-width 500 --candidates 1500 --max-program-size 150 --step-limit 5000`,
`--threads 2`, and walls of 240 s (A) / 180 s (C). Script:
`scripts/run_combinator_true_default_ab.sh`. Genuine SUCCESS uses section
37's rule (first solve below max-level). Any suite-C success on `cycle`,
`lis`, `longest_run` or `max_adjacent_gap` is re-checked with
`--benchmark`'s exhaustive check before being counted.

**Predictions:**
1. No regression on the 17 original problems: for each one, on-arm genuine
   successes >= off-arm - 1 (of 15), and the pooled suite-A total is not
   lower.
2. `reverse` and `graph_reachability` improve (on > off), as in section 42.
3. Suite C, the eight problems in the v2 product (`substring`, `palindrome`,
   `inversion_count`, `has_duplicate`, `is_sorted`, `count_descents`,
   `strictly_increasing`, `has_adjacent_equal`): off <= 3/15 each, on >= 13/15
   each.
4. `longest_run`, `max_adjacent_gap`, `cycle`, `lis`: 0/15 genuine in both
   arms (`cycle` after the exhaustive re-check).
5. Cost: no suite-A problem whose two arms both reach 15/15 becomes more
   than 2x slower in mean wall on the on arm.

Decision rule, fixed now: flip the default only if 1, 3 and 5 hold. If 1
fails on a specific problem, look at why before deciding.

## 47 (results). Combinator vs the true default: no regression, 8 problems 0-4/15 -> 15/15, one registered bound missed

`data/combinator_true_default_ab_20261003/` (2026-10-03, 12:29-14:02),
summarized with `scripts/summarize_prune_ab.py`. Genuine SUCCESS of 15
matched seeds per arm, mean wall in seconds:

| suite | problem | off | on | wall off / on |
|---|---|---|---|---|
| A | 15 of the 17 | 15/15 each | 15/15 each | all <= 0.3 s except factorial 90.6 / 81.1 |
| A | reverse | 15/15 | 15/15 | **3.82 / 0.28** |
| A | graph_reachability | 8/15 | 9/15 | 0.55 / 0.56 |
| A | sorting (slowest relative change) | 15/15 | 15/15 | 0.049 / 0.086 (1.8x) |
| C | substring, palindrome, inversion_count, count_descents, strictly_increasing | 0/15 each | **15/15 each** | 23-78 / 0.2-0.7 |
| C | has_duplicate / is_sorted / has_adjacent_equal | 2 / 4 / 1 of 15 | **15/15 each** | 40-80 / 0.4-0.6 |
| C | longest_run, max_adjacent_gap, lis | 0/15 | 0/15 | -- |
| C | cycle | 1/15 label | 10/15 label | **0 genuine in both** after `--benchmark`'s exhaustive check (1 and 10 rejected; wrong output at n = 1-2) |

**Scorecard:**
1. No regression on the 17: **held.** Every problem is equal or better.
2. reverse and graph_reachability improve: **partly.** graph_reachability
   +1 (within noise). reverse does not improve in successes, because the
   true default (4 vars) already solves it 15/15. Section 42's 2/5 off arm
   was an artifact of running at 8 vars. It is 13x faster, though.
3. Off <= 3/15 and on >= 13/15 on the eight v2-product problems: the on side
   **held** (15/15 all eight); the off bound **missed** on is_sorted (4/15).
   The miss is about the baseline, not the treatment.
4. longest_run, max_adjacent_gap, cycle, lis at 0 genuine: **held** (cycle
   only after the exhaustive check, the same lesson as sections 38 and 42).
5. No both-15/15 problem more than 2x slower: **held** (worst: sorting
   1.8x, 0.049 -> 0.086 s; count 1.3x; everything else equal or faster).

**Decision.** The registered rule required 1, 3 and 5 to hold. Prediction 3
missed only on its baseline bound (is_sorted off was 4/15, not <= 3). Its
substance, 15/15 on all eight, held. Whether that counts as passing is left
to an explicit decision, not assumed here; the default is unchanged by this
entry.

Caveats: suite-C successes other than cycle were counted with section 37's
rule, not re-checked exhaustively (section 42 re-checked only cycle too).
Walls ran 4 processes at once with `--threads 2`.

## 48. Combinator on by default

Decision on section 47: the CLI default of `--combinator-bias` is now 0.5
(`src/main.cpp`). Prediction 3's missed baseline bound was judged
irrelevant to its purpose; the treatment side (15/15 on all eight) held.
`GenConfig::combinatorBias` stays 0.0, so library users and unit tests are
unchanged (`make test` 115/115). Through `resolveNumVars`, every default
run now searches with at least 8 variables, even when `--num-vars` is
passed lower (a note is printed). `--combinator-bias 0` restores the
previous default exactly.

**Reproducibility break, stated plainly:** any earlier default-command
number (the README table of 2026-09-17, sections up to 47 that ran without
`--combinator-bias`) now needs `--combinator-bias 0` to reproduce.

Re-run of the modest battery with the new default
(`data/modest_battery_combinator_default_20261003/`, same script and
seeds as the README's sweep): all 17 problems confirmed, 0 rejections
anywhere (2026-09-17: 5 rejected on graph_reachability). Two changes stand
out. `reverse` drops from O(n²) at ~16 s/seed to **O(n)** at ~0.17 s/seed,
via the combinator's two-converging-pointers x swap block (the
quadratic solution was a skeleton detour). `graph_reachability` goes from
1 kept / 5 rejected to 3 kept / 0 rejected. Every other complexity class
is unchanged. The README table is regenerated from this run.

## 49. three_sum with high diversity and many seeds: only the skeleton, nothing without it

`scripts/run_three_sum_diversity.sh` (`data/three_sum_diversity_20261003/`,
not versioned). The question was whether high diversity, many seeds and a
large budget surface any 3SUM solution other than the hand-written
triple-loop skeleton (section 15). The configuration was `--max-n 8
--num-vars 10 --beam-width 1000 --candidates 3000 --max-program-size 300
--step-limit 20000`, section 45's diversity flags, and the combinator on (the
default since section 48). Each seed ran as its own `--benchmark
--benchmark-seeds 1` process with a 15 min wall, 48 seeds (40000-40047) per
arm. One process per seed is needed because `--benchmark` prints nothing
until the end, so a stalled seed would lose every seed batched with it; in
a 2-seed pilot at `--max-n 10`, both arms hit the 15 min wall.

| arm | finished / walled | benchmark kept / rejected | distinct loop structures |
|---|---|---|---|
| skel (default grammar) | 32 / 16 | 130 / 3, all fit O(n^2) | **1**: the ThreeSum skeleton's triple loop, in all 48 seeds; 263 texts differ only in inert code |
| noskel (`ALGOSEARCH_DISABLE_SKELETON=threesum`) | 0 / 48 | -- | -- |

The O(n^2) fit is the same small-range artifact the README footnote
describes; the program is the triple loop, O(n^3).

Without the skeleton, no seed produced a program that generalizes. 38
seeds stopped at N=4 and 10 at N=5, typically at 127/128 or 510-512/512,
with UNCONFIRMED per-level fits. The inspected N=5 champion counts
negatives in one loop and writes a flag; it is a statistical shortcut
on the instance distribution, not a relational search. The combinator
cannot express 3SUM in one block: no axis relates three elements, and
"sort + two converging pointers" would need two coordinated blocks plus a
two-sided pointer update the converging axis does not have. So this is
the expected outcome of section 39's rule (capability = the product's
span), now measured.

Conclusion: no interesting 3SUM solutions. Turning diversity and budget up
reproduces the skeleton; turning the skeleton off leaves nothing. An O(n^2)
3SUM is out of reach for the current grammar by construction. It would need
a new axis value (e.g. a two-sided converging update driven by a sum
comparison), under the freeze-predict-test discipline of sections 39-40.

## 50. Do solutions found on one problem become stepping stones for others? -- library frozen, 11 new problems, predictions registered before running

Question: does a "skeleton jump" between problems exist? In section 46 the
search built gnome and bubble sort from the combinator's single adjacent
pass plus one step found by mutation. The hypothesis tested here: if such
verified compositions are promoted automatically into the drawable
library, problems written later that need "that block + one more step"
become reachable. The promotion mechanism is NOT built yet; this entry
freezes the library and the test set and registers the predictions first.

**Freeze.** The library (hand-written skeletons + combinator axes) is the
state of this commit. No skeleton or axis is edited by hand until this
experiment closes.

**New problems** (all `ScalarFromArrayProblem`, whole array in, answer in
`output[0]`, exhaustive check over [-1, 2] for n <= 6). Each has a
hand-written Mini-C reference program in `tests/composition_problems_tests.cpp`
passing every generated, adversarial and exhaustive instance, so none is
out of the language's reach.

| type | problem | definition | natural solution |
|---|---|---|---|
| 1, sequential composition | `median` | lower median, sorted[(n-1)/2] | sort, then read |
| 1 | `second_smallest` | sorted[1] (duplicates count) | sort, then read |
| 1 | `count_distinct` | number of distinct values | sort, then count adjacent changes |
| 1 | `min_gap` | min difference between two values | sort, then adjacent min of differences |
| 1 | `max_gap_sorted` | max adjacent difference after sorting | sort, then adjacent max of differences |
| 2, state that resets | `max_subarray_sum` | max non-empty contiguous sum | Kadane |
| 2 | `longest_increasing_run` | longest strictly increasing contiguous run | adjacent pass with a resetting counter |
| classic | `max_profit` | max(0, max over i<j of a[j] - a[i]) | running minimum |
| classic | `count_leaders` | elements greater than everything to their right | right-to-left running maximum |
| classic | `has_majority` | some value occurs > n/2 times | count per element, or Boyer-Moore |
| classic | `trapping_rain_water` | water held between bars | prefix/suffix maxima |

**Arms** (15 matched seeds, the modest budget of section 47, `--benchmark`
exhaustive check for every claimed success):
- **D**: frozen default (selection-sort skeleton drawable, combinator on).
- **N**: D with `ALGOSEARCH_DISABLE_SKELETON=selectionsort,heapsort,mergesort`.
- **P**: N plus the promoted blocks. Training = section 46's runs, i.e.
  sorting with skeletons off plus the combinator. Promoted = verified,
  reduced solutions that are not a single existing block (gnome and bubble
  sort qualify; Fung's and exchange sort are single combinator draws and
  do not).

**Predictions:**
1. Baseline D. `second_smallest` >= 8/15: sort skeleton + one appended
   `arr[0] = arr[1]`, which growth can insert after the block. `median`
   <= 5/15 (needs a computed middle index). `count_distinct`, `min_gap`,
   `max_gap_sorted` <= 3/15 each: they need a sort followed by a second loop,
   and the combinator has no difference-of-adjacent relation.
2. Types 2 and classic, all arms: <= 2/15 each, 0/15 for `max_subarray_sum`
   and `trapping_rain_water`. Nothing in the library or in the promoted
   blocks carries a resetting accumulator, a right-to-left scan, or
   prefix/suffix maxima. These are the negative controls for P.
3. N <= D on every type-1 problem (it loses the only sort block).
4. **The hypothesis:** P > N on at least 3 of the 5 type-1 problems (by >= 5
   seeds each), with prediction 2's controls unchanged (+/- 2).
5. **The falsifier, stated now:** if D is <= 3/15 on most type-1 problems
   even though it already has a sort block, the bottleneck is composing
   AFTER a block, not having the block. In that case P is predicted to
   fail too, and the next lever is a composition mechanism (e.g. a
   "block then adjacent pass" combinator axis), not promotion.

## 50 (results, arms D and N). The falsifier fires: almost nothing solves even with a sort block, though composition after a block does happen

`scripts/run_stepping_stone_baseline.sh` (`data/stepping_stone_baseline_20261003/`,
not versioned; 330 runs, 22:02-23:15). A success is a seed with at least one
program surviving `--benchmark`'s exhaustive re-check.

| problem | D (sort skeleton on) | N (sort skeletons off) | notes |
|---|---|---|---|
| median | 0/15 | 0/15 | 2 / 1 rejected |
| second_smallest | 0/15 | **1/15** | D: 14 rejected, all a runtime error at n = 1 |
| count_distinct | 0/15 | 0/15 | |
| min_gap | 0/15 | 0/15 | stuck at N=2 (53/62) until the size budget ran out |
| max_gap_sorted | 0/15 | 0/15 | same |
| the six controls | 0/15 each | 0/15 each | |

**Composition after a block does happen.** D's 14 rejected `second_smallest`
programs are a sort block (the combinator's Fung sort) followed by
`v7 = arr[1]; ...; arr[0] = v7`. They are correct for every n >= 2 and crash
at n = 1 reading `arr[1]`. Their one flaw is section 50's n = 1 convention
(answer `sorted[0]`), a definition choice of mine that turned 14
near-correct compositions into failures. N's single genuine solution
composes two combinator blocks, neither a skeleton:

```
<Fung sort>
v6 = 1; while (v6 < n) { swap(arr[v6 - 1], arr[v6]); v6 = v6 + 1; }
```

An unconditional adjacent-swap pass rotates the sorted array left by one,
so `arr[0]` ends as `sorted[1]`, and for n = 1 the pass does nothing. It is
correct by exhaustive check and was found by search, not written.

**Scorecard:**
1. `second_smallest` D >= 8/15: **refuted** (0/15 genuine; 14/15 one edge case
   away). `median` <= 5 and the other three <= 3: held (all 0).
2. Controls <= 2 and Kadane / rain water at 0: held (all 0/15 in both arms).
3. N <= D on type 1: refuted on `second_smallest` (1 vs 0), held elsewhere
   (0 = 0).
5. **Falsifier: fires.** D is <= 3/15 on all five type-1 problems even
   though it has a sort block. By the rule registered before running, arm P
   (promotion) is predicted to fail too, and promotion is not built.

**Reading.** The obstacle is not having a sort block. Both arms have one
(D the skeleton, N the combinator's Fung sort), and in `second_smallest` the
search does append to it. It splits into two different gaps:
- **Edge cases of the appended step** (`second_smallest`, probably
  `median`): the read is right for n >= 2 and the search does not find the
  guard for small n. This is section 45's gnome-sort near miss again.
- **A missing relation** (`min_gap`, `max_gap_sorted`, `count_distinct`):
  none of these gets past N=2-4. The combinator has no "difference of
  adjacent elements" relation and no "count changes + 1", so the second
  block cannot be drawn whole, and mutation does not assemble it.

The next lever is therefore composition, as prediction 5 anticipated, and
specifically one of: (a) an axis value for the missing adjacent-difference
relation, under the freeze-predict-test discipline of sections 39-40; or
(b) enumeration of the small appended step, as section 41 did for the TSP
guard (`v3 = arr[k]` with a small-n guard is a tiny hole). Promotion
remains untested and is deprioritized.

## 51. Option (b): enumerating the small tail after a block -- predictions registered before running

Section 50's near misses were "sort, then read" programs that fail only on
an edge case (n = 1), and its registered falsifier pointed at composition
rather than promotion. Option (b) applies section 41's move to that tail:
enumerate it instead of waiting for mutation to find it.

**Mechanism** (`search/TailFiller.{hpp,cpp}`, `--tail-enum K`, opt-in, default
0). At every size step, each of the top K beam survivors is cut after
each of its last top-level statements (3 cut points), and the best tail
is appended. A tail is `arr[0] = E` or `if (n op k) arr[0] = E1 else
arr[0] = E2` (op in < <= == >= >, k in 1..3). E is a literal 0..2, a
scalar, or `arr[I]` with I in {0, 1, 2, n-1, n-2, n/2, (n-1)/2}. The
prefix runs once per instance (`Interpreter::run` gained an optional
final-scalars output), and every tail is evaluated on those end states.
The best tail is chosen on the instances of EVERY level seen so far, so a
small-n guard is selected for on the early levels. The results join the
grown pool and are scored like any other candidate. Off, the search is
byte-identical (checked on `second_smallest` and `median`, seed 8001,
against section 50's D logs). Unit tests: from a selection-sort prefix,
`fillTail` alone produces exhaustively correct `second_smallest` and
`median` programs. `make test` 129/129.

**Arms** (same 15 seeds 8000-8014, budget and per-seed `--benchmark` as
section 50; D and N reused from section 50):
- **DT** = D + `--tail-enum 10`
- **NT** = N + `--tail-enum 10`

**Predictions:**
1. `second_smallest`: DT >= 10/15 (D: 0/15).
2. `median`: DT >= 8/15 (D: 0/15).
3. `count_distinct`, `min_gap`, `max_gap_sorted`: <= 2/15 in DT and NT. A
   tail cannot contain a loop; these need option (a).
4. The six controls: <= 2/15 in DT and NT. No tail in the grammar
   computes them.
5. NT: `second_smallest` and `median` >= 8/15 each (the combinator's sort
   block is the prefix there, since no skeleton is available).

## 51 (results). Tail enumeration turns section 50's near misses into solutions: 0/15 -> 13/15, every prediction held

`scripts/run_tail_enum_ab.sh` (`data/tail_enum_ab_20261004/`, not versioned;
23:30-00:47). Seeds with at least one exhaustively verified program:

| problem | D (s.50) | **DT** | N (s.50) | **NT** |
|---|---|---|---|---|
| second_smallest | 0/15 | **13/15** | 1/15 | **10/15** |
| median | 0/15 | **13/15** | 0/15 | **10/15** |
| count_distinct, min_gap, max_gap_sorted | 0/15 | 0/15 | 0/15 | 0/15 |
| the six controls | 0/15 | 0/15 | 0/15 | 0/15 |

Typical winners: the sort skeleton followed by an enumerated tail.
- `second_smallest`: `if (n < 2) arr[0] = arr[0]; else arr[0] = arr[1];`, the
  small-n guard section 50's 14 near misses lacked, selected because the
  tail is chosen on every level seen.
- `median`: `arr[0] = arr[(n - 1) / 2];`, exactly the definition.

In NT the prefix is the combinator's sort (no skeleton), at 10/15.

**Scorecard:** 1 (second_smallest DT >= 10): held, 13. 2 (median DT >= 8):
held, 13. 3 (the three loop-needing problems <= 2): held, 0. 4 (controls
<= 2): held, 0. 5 (NT >= 8 on both): held, 10 and 10.

**Conclusion.** Section 50's falsifier was right about where the gap was:
composition after a block, not the block. Enumerating the small appended
step closes it whenever that step is loop-free. This is the same pattern as
section 41 (mutation could not reach an isolated point; enumeration
does), now on general array problems and inside the ordinary search. The
three problems that need a second LOOP after the sort stay at 0, which is
option (a)'s target. `--tail-enum` stays opt-in until an A/B on the
original problems shows it costs nothing.

## 52. Option (a): step axes in the combinator -- frozen, post-freeze problems written, predictions registered before running

**Axes** (commit e592de3, frozen before any test problem was written):
`--combinator-step` (opt-in, byte-identical off) adds two relation values on
the adjacent-pair loop, drawn as 2 of 7 relations there:
- **BestStep**: running min or max of `elem(i) - elem(i-1)`, initialized
  from the first pair when there is one, else 0.
- **CountSegments**: 1 + the number of adjacent pairs satisfying a drawn
  comparison.

They were designed by looking at section 50's `count_distinct`, `min_gap`
and `max_gap_sorted`, so those three are **design targets**: a success on
them is not evidence of generality.

**Post-freeze problems** (written after e592de3, reference programs in
`tests/composition_problems_tests.cpp`, exhaustive check n <= 6):
`max_rise` (largest adjacent step, may be negative), `min_step` (smallest
step), `count_runs` (maximal runs of equal values), `count_ascending_runs`
(maximal strictly increasing runs).

**Phase-1 reachability** (20,000 sampled combinator blocks, correct on
every exhaustive input for n = 1..6), step axes off / on: max_rise 0 / 55,
min_step 0 / 74, count_runs 0 / 27, count_ascending_runs 0 / 35. The three
design targets are 0 / 0: each needs TWO blocks, a sort and then a step
pass. The controls max_subarray_sum and longest_increasing_run are 0 / 0.

**Arms:** S0 = default, S1 = default + `--combinator-step`. Both use 15
seeds (8000-8014), the modest budget, per-seed `--benchmark` with the
exhaustive check, and `--max-n 8` (5 for sorting).

**Predictions:**
1. The four post-freeze problems: S1 >= 13/15 each, S0 <= 2/15 each.
2. Design targets: S1 >= 5/15 each, S0 0/15. This needs search to compose
   a sort block and a step block, which no single draw provides, so it is
   the harder claim.
3. Controls (max_subarray_sum, longest_increasing_run, trapping_rain_water):
   0/15 in both arms.
4. No dilution on the adjacent-pass problems the step relations compete
   with (is_sorted, count_descents, strictly_increasing, has_adjacent_equal)
   or on sorting: S1 >= S0 - 1 on each.

## 52 (results). Step axes: post-freeze problems 15/15, design targets reached by composing two blocks, no dilution

`scripts/run_step_axes_ab.sh` (`data/step_axes_ab_20261004/`, not versioned;
00:50-01:37, 450 runs). Seeds with an exhaustively verified program:

| group | problem | S0 (default) | S1 (+ step axes) |
|---|---|---|---|
| post-freeze | max_rise, min_step, count_runs | 0/15 each | **15/15 each** |
| post-freeze | count_ascending_runs | 4/15 | **15/15** |
| design target | count_distinct | 0/15 | 4/15 |
| design target | min_gap | 0/15 | 5/15 |
| design target | max_gap_sorted | 0/15 | **10/15** |
| control | max_subarray_sum, longest_increasing_run, trapping_rain_water | 0/15 | 0/15 |
| dilution | is_sorted, count_descents, strictly_increasing, has_adjacent_equal, sorting | 15/15 each | 15/15 each |

**The design targets are solved by composition across two blocks**, which
no single draw contains (phase-1 reachability 0). A typical `max_gap_sorted`
winner is the selection-sort skeleton followed by a BestStep block. One
mutation dropped its `acc = 0` init, which is harmless because an unset
scalar is 0. `count_distinct`'s winner is the sort followed by a
CountSegments block with `!=`. These are the stepping-stone compositions
section 50 asked about, now reachable because the second block exists as
a drawable unit.

S0's 4/15 on `count_ascending_runs` comes from bloated nested-loop programs
that happen to be exhaustively correct (e.g. counting pairs with a
mutated swap), not from an adjacent pass.

**Scorecard:**
1. Post-freeze S1 >= 13/15: **held** (15/15 x 4). The S0 <= 2/15 bound
   **missed** on count_ascending_runs (4/15), a baseline miss, like
   section 47's is_sorted.
2. Design targets S1 >= 5/15: **held** on min_gap (5) and max_gap_sorted
   (10); **missed** on count_distinct (4/15).
3. Controls 0 in both arms: **held**.
4. No dilution: **held** (15/15 in both arms on all five).

**Conclusion.** Option (a) works and generalizes to problems written after
its axes were frozen, with no measured cost on the problems it competes
with. With (b) (section 51), section 50's question is answered positively
for this grammar. A solution needing "block + step" is reachable when the
step is either a loop-free tail (enumeration, b) or a drawable block (an
axis value, a), and search composes it with a sort block it did not get
together with it. Still out of reach: state that resets mid-loop (Kadane,
longest increasing run), and prefix/suffix maxima (rain water).
`--combinator-step` and `--tail-enum` stay opt-in until a matched battery
on the full problem set (section 47's protocol) decides about defaults.

## 53. Should `--tail-enum` and/or `--combinator-step` become defaults? -- predictions registered before running

The protocol is section 47's, extended to four arms so each flag is
decided on its own: **base** (current default), **T** (+ `--tail-enum 10`),
**S** (+ `--combinator-step`) and **TS** (both). The runs cover all 44
registered array/scalar problems: the 17 originals at the modest
battery's max-n / num-vars with a 240 s wall, and the 27 newer ones at
max-n 8 (substring 6, cycle and lis 5) with a 180 s wall. Each arm gets
10 fresh matched seeds (9000-9009), the modest budget, and one `--benchmark
--benchmark-seeds 1` process per run, so a success is a seed whose program
survives the independent (exhaustive where defined) re-check. Script:
`scripts/run_default_candidates_ab.sh`.

**Predictions:**
1. No regression: on every problem base solves, T, S and TS each score
   >= base - 1 (of 10).
2. T gains on second_smallest and median (>= base + 5 each) and nowhere
   else by more than 2.
3. S gains on max_rise, min_step, count_runs, count_ascending_runs (>= base
   + 5 each) and on max_gap_sorted (>= base + 3).
4. TS >= max(T, S) - 1 on every problem: the two do not interfere.
5. Cost: on problems where both arms solve 10/10, no flag makes mean
   search wall more than 2x base (tail enumeration adds work every size
   step, so T and TS are the ones at risk).

**Decision rule, fixed now:** a flag becomes default if 1 and 5 hold for
its arm and its gain prediction (2 for T, 3 for S) holds. Both become
default if TS also satisfies 1, 4 and 5.

## 53 (results). `--combinator-step` becomes the default; `--tail-enum` does not (yet)

`data/default_candidates_ab_20261004/` (not versioned; 07:44-10:27, 1760
runs). Verified successes of 10 matched seeds:

| problem(s) | base | T | S | TS |
|---|---|---|---|---|
| 15 of the 17 originals; substring, palindrome, inversion_count, has_duplicate, is_sorted, count_descents, strictly_increasing, has_adjacent_equal | 10 each | 10 each | 10 each | 10 each |
| three_sum / graph_reachability | 8 / 6 | 8 / 6 | 8 / 6 | 8 / 6 |
| median | 0 | **8** | 0 | **7** |
| second_smallest | 2 | 4 | 1 | **8** |
| count_distinct / min_gap / max_gap_sorted | 0 / 0 / 0 | 0 / 0 / 0 | 2 / 3 / **5** | 1 / 4 / **5** |
| max_rise / min_step / count_runs | 0 each | 0 each | **10 each** | **10 each** |
| count_ascending_runs | 4 | **1** | **10** | **10** |
| longest_run, max_adjacent_gap, cycle, lis, max_subarray_sum, longest_increasing_run, max_profit, count_leaders, has_majority, trapping_rain_water | 0 each | 0 each | 0 each | 0 each |

Cost on problems all four arms solve 10/10: the worst mean-search-wall
ratio vs base is S 1.42x (is_sorted, 0.81 -> 1.15 s), T 1.10x and TS 1.10x
(factorial, 131 -> 145 s); TS 1.29x on count_descents. Everything else is
within +/- 0.05 s.

**Scorecard:**
1. No regression: S held (worst: second_smallest 2 -> 1, within the
   allowed 1). TS held. **T failed** on count_ascending_runs (4 -> 1).
   Base's 4 are the bloated, accidentally correct nested-loop programs of
   section 52, so this is plausibly noise, but the rule was fixed in
   advance.
2. T gains on second_smallest and median >= 5: median held (+8);
   **second_smallest failed** (+2). It is +6 in TS, where the step axes
   give the tail more prefixes to work on.
3. S gains: held on all four step problems (+10, +10, +10, +6) and on
   max_gap_sorted (+5).
4. TS >= max(T, S) - 1: held everywhere.
5. Cost <= 2x: held for every arm.

**Decision (by the rule registered in section 53):** `--combinator-step`
is now the default (`--no-combinator-step` restores the previous one).
`--tail-enum` stays opt-in: its own arm failed predictions 1 and 2,
although TS, the arm that matters once S is default, satisfies 1, 4 and 5
and is the best arm on median/second_smallest. The natural follow-up is
section 47's protocol again with S as the new base: base vs base +
`--tail-enum`, more seeds (the second_smallest and count_ascending_runs
differences here are 2-3 of 10).

The 17 original problems are unchanged by S (all equal to base), so the
README table of section 48 still describes the default.

## 54. `--tail-enum` on top of the new default -- predictions registered before running

Section 53 kept `--tail-enum` opt-in on its own arm's numbers. With the
step axes now default, the relevant comparison is the new **base** vs
**T** = base + `--tail-enum 10`. Both arms use 30 fresh matched seeds
(11000-11029), section 53's budget and per-seed `--benchmark`, and the 34
problems some arm of section 53 solved. The 10 problems every arm left at
0 are excluded: they cannot regress and no gain is predicted for them.
Script: `scripts/run_tail_enum_default_ab.sh`.

**Predictions:**
1. No regression: T >= base - 3 (of 30) on every problem.
2. Gains: median T >= base + 15; second_smallest T >= base + 10.
3. Cost: on problems both arms solve 30/30, T's mean search wall <= 2x
   base.

**Decision rule, fixed now:** `--tail-enum 10` becomes the default if 1, 2
and 3 all hold.

## 54 (results). `--tail-enum 10` becomes the default: median 0 -> 23/30, second_smallest 2 -> 15/30, no regression

`data/tail_enum_default_ab_20261004/` (not versioned; 11:11-12:41, 2040
runs). Verified successes of 30 matched seeds:

| problem(s) | base | T |
|---|---|---|
| 27 problems | 30/30 each | 30/30 each |
| three_sum / graph_reachability | 27 / 13 | 27 / 12 |
| count_distinct / min_gap / max_gap_sorted | 7 / 10 / 10 | 7 / 10 / 11 |
| **median** | 0 | **23** |
| **second_smallest** | 2 | **15** |

Cost on the 27 problems both arms solve 30/30: mean search wall ratio
T/base between 0.88x and 1.27x (worst has_adjacent_equal 1.04 -> 1.32 s;
factorial 136 -> 148 s, 1.09x).

**Scorecard:** 1 (T >= base - 3 everywhere): held, worst graph_reachability
13 -> 12. 2 (median >= +15, second_smallest >= +10): held, +23 and +13. 3
(cost <= 2x): held, worst 1.27x.

**Decision:** by the rule fixed in section 54, `--tail-enum 10` is now the
default (`--tail-enum 0` restores the previous one). Section 53's arm-T
failures were small-sample effects: with 30 seeds count_ascending_runs is
30/30 in both arms, and second_smallest's gain is +13. The 17 original
problems are equal in both arms, so the README table still describes the
default.

## 55. Stateful combinator pieces (Kadane, rain water, LIS) -- frozen, post-freeze problems written, predictions registered before running

**Pieces** (commit b30674a, frozen before any test problem was written).
`--combinator-state` (opt-in, byte-identical off) draws a quarter of
combinator blocks from four pieces whose state carries across iterations.
Each works on the whole-array or the length-prefixed layout.
- **RunBest**: adjacent pass; `run = cmp(prev, cur) ? run + 1 : 1`; best
  run.
- **ResetSum**: single pass; `cur = (cur < 0) ? a : cur + a` (or the
  mirror for a min); best cur, i.e. Kadane's restart.
- **EarlierCellsBest**: `aux[i] = w + best aux[j]` over `j < i` with
  `cmp(a[j], a[i])`, with w = 1 or a[i]; best aux[i]. This is the "read
  several already-computed cells" primitive section 24 predicted DP needs.
- **PrefixSuffix**: prefix best in aux, then a right-to-left pass with a
  running suffix best r, adding `combine(aux[i], r) - a[i]` or its
  negation, with combine = min or max. It is the most problem-shaped
  piece: trapping rain water is one draw of it.

**Design targets** (looked at while designing, so not evidence of
generality): longest_run, longest_increasing_run, max_subarray_sum, lis,
trapping_rain_water. **Post-freeze problems** (written after b30674a,
references in `tests/composition_problems_tests.cpp`):
- `longest_non_increasing_run` (RunBest with >=);
- `min_subarray_sum` (ResetSum, min);
- `longest_decreasing_subsequence` (EarlierCellsBest, >);
- `max_sum_increasing_subsequence` (EarlierCellsBest with w = a[i]);
- `ceiling_water` (upside-down rain water: PrefixSuffix with min-prefix,
  max-combine, a[i] minus it).

**Phase-1 reachability** (20,000 blocks, every generated, adversarial and
exhaustive instance for n = 1..6), off / on:
- design targets: longest_run 0 / 94, longest_increasing_run 0 / 114,
  max_subarray_sum 0 / 322, trapping_rain_water 0 / 71, lis 0 / 57;
- post-freeze: longest_non_increasing_run 0 / 98, min_subarray_sum 0 / 318,
  longest_decreasing_subsequence 0 / 47,
  max_sum_increasing_subsequence 0 / 63, ceiling_water 0 / 76.

**Arms:** base (current default) vs base + `--combinator-state`. Both use 15
fresh matched seeds (12000-12014), the modest budget, per-seed
`--benchmark`, `--max-n 8` (lis 5, sorting 5). Script:
`scripts/run_state_pieces_ab.sh`.

**Predictions:**
1. The five post-freeze problems: on >= 12/15 each, base <= 2/15 each.
2. The five design targets: on >= 12/15 each (base 0 in sections 53-54).
3. No dilution on is_sorted, count_descents, max_rise, count_runs, median,
   second_smallest, max_gap_sorted, sum, max, sorting: on >= base - 2 each.
4. Cost: on problems both arms solve 15/15, mean search wall <= 2x base.

## 55 (results). Stateful pieces: every post-freeze problem and every design target solved, no dilution

`scripts/run_state_pieces_ab.sh` (`data/state_pieces_ab_20261004/`, not
versioned; 14:26-15:15, 600 runs). Verified successes of 15 seeds:

| group | problem | base | + state |
|---|---|---|---|
| post-freeze | longest_non_increasing_run | 0 | **14** |
| post-freeze | min_subarray_sum, longest_decreasing_subsequence, max_sum_increasing_subsequence, ceiling_water | 0 each | **15 each** |
| design target | longest_run, longest_increasing_run, max_subarray_sum, trapping_rain_water | 0 each | **15 each** |
| design target | lis | 0 | **13** |
| dilution | is_sorted, count_descents, max_rise, count_runs, sum, max, sorting | 15 each | 15 each |
| dilution | median / second_smallest / max_gap_sorted | 12 / 9 / 6 | 10 / 10 / **13** |

Cost on the both-15/15 problems: 0.89x-1.19x, except count_runs at 1.78x
(0.43 -> 0.77 s).

The `lis` winners are the textbook DP, drawn whole by EarlierCellsBest on
the length-prefixed layout:
`aux[i] = 1 + max{aux[j] : j < i, a[j] < a[i]}`, answer the max aux[i]. This
is the "sixth primitive" section 24 predicted no skeleton provides, and
`lis` had been the held-out ruler since section 25, at 0 in every
configuration until now.

**Scorecard:** 1 (post-freeze on >= 12, base <= 2): held (14, 15, 15, 15,
15 vs 0). 2 (design targets on >= 12): held (15, 15, 15, 15, 13). 3 (no
dilution, >= base - 2): held (worst median 12 -> 10). 4 (cost <= 2x): held
(worst 1.78x).

**Reading.** Each piece generalized past the problem it was designed
from. That includes PrefixSuffix, the most problem-shaped of the four: the
upside-down `ceiling_water` (min prefix, max combine, sign flipped) is
15/15. As with sections 52-54, the new vocabulary is small and
hand-written, and the search composes and parametrizes it.
`--combinator-state` stays opt-in until a full-battery default decision.
Section 47's protocol is the next step, together with the skeleton
isolation battery planned for after this work.

## 56. Stateful pieces as default + which hand-written skeletons are indispensable -- predictions registered before running

One battery, two questions. It covers all 49 registered array/scalar
problems at section 53's max-n, walls and modest budget, with 10 fresh
matched seeds (13000-13009) and per-seed `--benchmark`. Script:
`scripts/run_state_isolation_ab.sh`. Arms:
- **base**: current default.
- **ST**: base + `--combinator-state`.
- **STX**: ST with every hand-written skeleton disabled via
  `ALGOSEARCH_DISABLE_SKELETON` (all 17: average, binarysearch, counting,
  divideconquer, dtw, graphreachability, heapsort, mergesort,
  nesteddivisor, nesteddivisornative, recurrence, repeatedmultiply,
  selectionsort, shortestpath, threesum, tspbranchandbound,
  tspbruteforce).

**Question A, stateful pieces as default (ST vs base).**
1. No regression: ST >= base - 1 (of 10) on every problem.
2. Gains: the ten stateful problems of section 55 >= base + 7 each.
3. Cost: on problems both solve 10/10, ST's mean search wall <= 2x base.
Decision rule: `--combinator-state` becomes default if 1, 2 and 3 hold.

**Question B, indispensable skeletons (STX vs ST).**
4. Predicted to drop by >= 3 of 10 without skeletons:
   - gcd, factorial, fibonacci (recurrence);
   - is_prime (nested divisor);
   - power (repeated multiply);
   - average;
   - three_sum;
   - graph_reachability.
5. Predicted to stay within -1: every other problem, i.e. the combinator
   (with step and state pieces) and tail enumeration cover sum, min, max,
   count, contains, contains_ordered, reverse, single_number, sorting and
   all 32 newer problems.

Decision rule for B, fixed now. A skeleton is **removed** if every problem
in this battery that it serves stays within -1 in STX. A skeleton serving
a problem that drops by >= 2 is **kept**. Skeletons whose problems are not
in this battery are kept as untested: dtw, shortestpath, tspbruteforce,
tspbranchandbound (problems with their own layouts), and binarysearch,
whose O(log n) contains_ordered result is a speed property this
success-only battery cannot see. When a dropping problem maps to more
than one skeleton, all the candidates are kept until a per-skeleton
follow-up.

## 56 (results). Isolation: 8 problems need their skeletons, 4 skeletons are removable; stateful default narrowly fails its rule

`data/state_isolation_ab_20261004/` (not versioned; 16:59-18:56, 1470
runs). Verified successes of 10 matched seeds, base / ST / STX:

| problem(s) | base | ST | STX |
|---|---|---|---|
| sorting, min, max, sum, contains, contains_ordered, count, reverse, single_number, and the 8 v2-product problems | 10 each | 10 each | 10 each |
| is_prime, gcd, factorial, fibonacci, average, power | 10 each | 10 each | **0 each** |
| three_sum / graph_reachability | 8 / 6 | 8 / **4** | **0 / 0** |
| median / second_smallest | 8 / 8 | 7 / **6** | 10 / 10 |
| count_distinct / min_gap / max_gap_sorted | 0 / 4 / 6 | 7 / 6 / 8 | **1 / 3 / 6** |
| the ten stateful problems (section 55) | 0 each | 10 each | 10 each |
| step problems (max_rise, min_step, count_runs, count_ascending_runs) | 10 each | 10 each | 10 each |
| max_adjacent_gap, cycle, max_profit, count_leaders, has_majority | 0 | 0 | 0 (count_leaders 1) |

**Question A (stateful pieces as default).** Prediction 2 held (+10 on all
ten stateful problems, lis included) and prediction 3 held (worst cost
1.84x, count, 0.2 s). **Prediction 1 failed** on two problems by exactly 2:
graph_reachability 6 -> 4 and second_smallest 8 -> 6. Both look like the
sample noise section 54 exposed in section 53 (second_smallest is 10/10 in
STX), but the rule was fixed in advance. `--combinator-state` stays
opt-in pending a 30-seed retest of those two problems, as section 54 did
for tail enumeration.

**Question B (indispensable skeletons).**
- **Prediction 4 held exactly.** All eight predicted problems drop to 0
  without skeletons: is_prime, gcd, factorial, fibonacci, average, power,
  three_sum, graph_reachability.
- **Prediction 5 failed on three problems.** count_distinct (7 -> 1),
  min_gap (6 -> 3) and max_gap_sorted (8 -> 6) drop. Section 52's winners
  for them are the selection-sort skeleton followed by a step block, and
  without the skeleton the combinator's own sorts compose with the step
  pass much less often. So selectionsort is a composition stepping stone,
  not just a sorting solution.
- Everything else, all 32 newer problems included, stays within -1.

Applying section 56's rule:
- **Kept (a problem they serve drops >= 2):**
  - recurrence (gcd, fibonacci);
  - nesteddivisor and nesteddivisornative (is_prime; ambiguous between
    the two, so both kept);
  - repeatedmultiply (factorial, power);
  - average;
  - threesum;
  - graphreachability;
  - selectionsort (count_distinct, min_gap, max_gap_sorted).
- **Kept as untested:** dtw, shortestpath, tspbruteforce,
  tspbranchandbound, binarysearch.
- **Removable (every problem they serve stays within -1):**
  - counting (sum, min, max, count, contains, single_number);
  - heapsort (sorting);
  - mergesort and divideconquer (both need `--recursion-call-bias`, so
    they never fire in the default).

Caveat on the removable set, noticed after the run: heapsort and
mergesort are the project's only O(n log n) sorts (sections 14 and 25).
That is the same speed-not-success argument that exempted binarysearch,
but the rule as registered did not exempt them. Whether to remove them
is left as an explicit decision.

## 57. Two skeletons removed: `counting` and `divideconquer`

Decision on section 56 (with the user): remove `counting` and
`divideconquer`, which every problem they serve proved not to need.
Keep heapsort and mergesort despite passing the removal rule, because
they are the only O(n log n) sorts. That is the same speed argument that
exempted binarysearch, and a success-only battery cannot see it.
Removed from `ProgramGenerator.cpp`: `genCountingLoopBlock`,
`genDivideConquerSkeletonBlock`, their skeleton options and the
accumulator helpers only `counting` used. Also removed the test
`loop_accumulator_bias_produces_a_counting_loop_when_forced`, which tested
the removed skeleton. `make test` 141/141.

Removing one skeleton is not the configuration section 56 measured
(STX had every skeleton off) and changes the default's RNG stream. So the
README's modest battery was re-run on the new default
(`data/modest_battery_skeleton_pruned_20261004/`, not versioned). All 17
problems are still confirmed with 0 rejections, in the same complexity
classes as section 48's table (5 kept everywhere except fibonacci 4 and
graph_reachability 3).

The skeleton library is now 15: recurrence, nesteddivisor,
nesteddivisornative, selectionsort, repeatedmultiply, average, heapsort,
threesum, graphreachability, shortestpath, tspbruteforce,
tspbranchandbound, dtw, binarysearch, mergesort.

## 58. `--combinator-state` retest on its two near misses -- predictions registered before running

In section 56, `--combinator-state` met its gain and cost predictions but
missed no-regression by exactly 2 of 10 on graph_reachability (6 -> 4) and
second_smallest (8 -> 6). As section 54 did for tail enumeration, those
two problems are retested with 30 fresh matched seeds (14000-14029),
**base** (the current default, now without `counting`/`divideconquer`,
section 57) vs **ST** = base + `--combinator-state`, per-seed
`--benchmark`. Script: `scripts/run_state_retest_ab.sh`.

**Prediction:** ST >= base - 3 (of 30) on both problems.

**Decision rule, fixed now:** if it holds, `--combinator-state` becomes the
default (section 56's gain and cost predictions already held). If it
fails on either problem, it stays opt-in.

## 58 (results). Not noise: `--combinator-state` halves graph_reachability, stays opt-in

`data/state_retest_ab_20261004/` (not versioned; 19:23-19:31, 120 runs).
Verified successes of 30 matched seeds:

| problem | base | ST |
|---|---|---|
| graph_reachability | 16 | **8** |
| second_smallest | 30 | 27 (3 rejected) |

**Prediction (ST >= base - 3 on both): failed** on graph_reachability by 8.
second_smallest is within the bound. Unlike section 53's tail-enumeration
misses, which disappeared at 30 seeds, this one grew from -2/10 to -8/30:
a real cost. **Decision: `--combinator-state` stays opt-in.**

Likely mechanism, not yet tested: graph_reachability is only reachable
through its skeleton (section 56: 0/10 without skeletons), and the
skeleton is assembled over several size steps. The stateful pieces take a
quarter of every combinator draw, adding a family of large, fully scoring
blocks on the early levels that compete for beam slots with the
partially built skeleton. Candidate fixes for a follow-up, each needing
its own matched A/B: a smaller state share (e.g. 1/8 instead of 1/4), or
drawing the stateful pieces only on problems whose layout is a plain
sequence. graph_reachability's flat adjacency layout is not one, but
nothing in a problem-agnostic generator knows that.

## 59. Adaptive family weights against dilution -- design and predictions registered before implementing

Section 58 measured the cost of adding a family of blocks. Every new
family takes a fixed share of the draws on every problem, so it crowds out
the families a problem actually needs: graph_reachability went from 16 to
8 of 30 when the stateful pieces got a quarter of the combinator draws.
If the library is to grow (by hand or, later, by automatic extraction),
the generator needs shares that adapt to the problem during the run,
instead of shares fixed in the source.

**Mechanism (`--adaptive-families`, off by default).**
- **Arms.** Each top-level block family is one arm: every hand-written
  skeleton, plain combinator blocks and stateful combinator blocks. Plain
  grammar statements are not an arm; their share is unchanged.
- **Tagging.** While a candidate is generated or grown, the generator
  records which families it drew. BeamSearch keeps that set with the
  candidate, and mutated or crossed-over children inherit the union of
  their parents' sets.
- **Credit is progress, not survival.** A family gets one point each time
  a candidate containing it sets a new best (score, then size) within a
  level. Credits decay by 0.9 per level. Rewarding beam survival was
  rejected on purpose: section 58's crowding is a survival effect, and
  rewarding survival would amplify the very family causing it.
- **Weights.** effective_f = static_f * m_f, with m_f = (1 + credit_f) /
  (1 + mean credit), clamped to [0.5, 4]. The 0.5 floor guarantees that no
  family, in particular a skeleton that only pays off after several size
  steps, ever drops below half its static share. The two existing draws
  use the effective weights: the weighted skeleton roll, and the
  plain-vs-stateful split inside the combinator.
- **Logging.** At the end of a run, the final m_f of every arm goes to the
  log, so the mechanism can be checked and not just the outcome.
- **Byte identity.** With the flag off, programs and the RNG stream are
  unchanged. A test in `tests/combinator_tests.cpp` enforces this, as for
  earlier flags.

**Experiment.**
- **Phase A** (the regression): graph_reachability and second_smallest,
  30 fresh matched seeds (15000-15029), section 58's budgets. Arms:
  - **base**: current default;
  - **ST**: base + `--combinator-state`;
  - **ST8**: ST with the stateful share cut to 1/8. This is the one-line
    manual fix and serves as the control.
  - **STA**: ST + `--adaptive-families`.
- **Phase B** (the full battery, run only if STA passes phase A): all 49
  array/scalar problems, 10 fresh matched seeds (16000-16009), section
  56's budgets, base vs STA.

**Predictions.**
1. ST replicates section 58: ST <= base - 5 on graph_reachability.
2. STA >= base - 3 (of 30) on both phase A problems.
3. ST8 lands between ST and base on graph_reachability. This is not a
   pass/fail prediction; it is the control for rule (b) below.
4. In STA's graph_reachability successes, the final m_f of
   graphreachability is >= 1. The skeleton is credited, not merely
   protected by the floor.
5. Phase B, STA vs base:
   - STA >= base - 1 on every problem;
   - STA >= base + 7 on each of the ten stateful problems of section 55;
   - on problems both arms solve 10/10, mean wall <= 2x base.

**Decision rule, fixed now.**
- **(a)** If 2 and 5 hold, `--adaptive-families` and `--combinator-state`
  both become the default.
- **(b)** If ST8 also satisfies prediction 2's bound, the simpler fix
  (state share 1/8) goes through phase B instead. The bandit is then
  recorded as unnecessary for this case and stays opt-in, as
  infrastructure for future families.
- **(c)** If prediction 2 fails for STA, adaptive credit as designed is
  refuted for this regression. If prediction 4 also fails, the cause is
  the credit signal, not the shares.
- **(d)** If prediction 1 fails (no replication), section 58's regression
  was not robust. That is recorded, and the A/B is re-read from scratch.

## 59 (results, phase A). The bandit credits the right skeleton but does not fix the crowding; the 1/8 share does

`data/adaptive_families_ab_20261004/` (not versioned; 22:41-22:58, 240
runs). Verified successes of 30 matched seeds (15000-15029):

| problem | base | ST | ST8 | STA |
|---|---|---|---|---|
| graph_reachability | 15 | 11 | **15** | 11 |
| second_smallest | 29 | 30 | 28 | 29 |

1. **Failed by one.** ST <= base - 5 did not hold: the result was -4.
   Section 58's regression shrank on fresh seeds, but it is still there.
   Pooled over both retests (60 seeds), ST costs 31 -> 19 on
   graph_reachability.
2. **Failed.** STA = 11 on graph_reachability, base - 4. Adaptive weights
   recovered nothing over ST.
3. **ST8 did not land in between.** It matched base (15) and is within the
   bound on second_smallest (28 >= 26).
4. **Held.** The graphreachability multiplier ended >= 1 in every STA run
   (2.4-3.8), successful or not. The credit signal finds the right
   skeleton.

**Why STA failed although 4 held.** The two existing draws are not where
the crowding happens:
- The graph skeleton was boosted only relative to the other skeletons.
  The gates that decide whether a skeleton or a combinator block is drawn
  at all (`loopAccumulatorBias`, `combinatorBias`) were not arms.
- The stateful share is a ratio between two families that were both
  never credited on graph_reachability. Both ended at the same
  multiplier (about 0.85-0.9), so the stateful quarter stayed a quarter.

Normalizing by the mean credit cannot push down one uncredited family
relative to another. Cutting the uncredited family's share at the source
can, and that is what ST8 does.

**Decision (rules (b) and (c)).**
- Adaptive credit as designed is refuted for this regression.
  `--adaptive-families` stays opt-in.
- It is not refuted as a signal (prediction 4). A redesign would need
  the top-level gates as arms too.
- ST8 meets prediction 2's bound, so per rule (b) **ST8** (stateful share
  1/8) goes to phase B in place of STA.

## 59 (results, phase B). ST8 solves every stateful target but misses no-regression again on graph_reachability

`data/state_share_phase_b_20261004/` (not versioned; 23:00-00:20, 980
runs, 54 wall timeouts, all on problems neither arm solves). Script:
`scripts/run_state_share_phase_b.sh`. Verified successes of 10 matched
seeds (16000-16009), base / ST8:

| problem(s) | base | ST8 |
|---|---|---|
| the ten stateful problems of section 55 | 0 each | 10 each (lis 9) |
| graph_reachability | 8 | **5** |
| min_gap / max_gap_sorted / count_distinct | 6 / 8 / 2 | **4** / 7 / 4 |
| palindrome / three_sum | 10 / 9 | 9 / 9 |
| every other solved problem (32) | 10 each | 10 each |
| max_adjacent_gap, cycle, max_profit, count_leaders, has_majority | 0 | 0 |

Prediction 5:
- **(i) Failed:** no-regression within -1 does not hold.
  graph_reachability drops 8 -> 5 and min_gap drops 6 -> 4.
- **(ii) Held:** the ten stateful problems go up by 9-10 each.
- **(iii) Failed, narrowly:** the worst cost ratio on problems both arms
  solve 10/10 is 2.11x, on count_ascending_runs (0.61 s -> 1.28 s).
  Everything else is <= 1.4x.

**Decision: by rule (a), ST8 does not become the default either.**
`--combinator-state` stays opt-in at either share.

graph_reachability is the same problem for the third time in a row. The
evidence across the three experiments:

| experiment | base | with state |
|---|---|---|
| section 58 (30 seeds) | 16 | 8 |
| phase A, ST (30 seeds) | 15 | 11 |
| phase A, ST8 (30 seeds) | 15 | 15 |
| phase B, ST8 (10 seeds) | 8 | 5 |

Pooled at 1/8, ST8 gives 20/40 against base 23/40. That is a much smaller
cost than 1/4's 19/60 against 31/60, but probably not zero. The share
knob trades graph_reachability against nothing else: no other skeleton
problem moved. A fix aimed at that specific crowding (a guarded beam
quota for a partially built skeleton, or the gates as adaptive arms, see
the phase A analysis) is the open follow-up; the share alone is not
enough for a no-regression default.

## 60. A protected beam quota for skeleton lineages -- predictions registered before implementing

Sections 58-59 locate the conflict: graph_reachability's skeleton is built
over several size steps, and while it is incomplete its lineage loses beam
slots to fully scoring blocks, the stateful ones in particular. Neither
cutting the stateful share (ST8) nor reweighting draws (STA) removed the
cost. This section protects the lineage directly, in selection rather
than in generation.

**Mechanism (`--skeleton-quota R`, 0 = off and byte-identical).**
- A fraction R of the beam's elite slots becomes a niche, alongside the
  existing shape, behavior and score-band niches.
- The niche is filled round-robin over the hand-written skeleton families
  (section 59's lineage tags, bits 0-14). Each family in turn gets its
  best-ranked candidate not already in the beam, until the quota is full
  or no tagged candidate is left.
- A skeleton lineage that is temporarily outscored keeps a few slots
  instead of dropping out. No generation draw changes.
- R = 0.1 (50 slots at beam width 500), fixed before running.

**Phase A.** graph_reachability, second_smallest and min_gap (min_gap is
phase B's other drop; its winners are selectionsort + a step block,
section 56). 30 fresh matched seeds (17000-17029), the section 58
budgets, plus section 56's for min_gap. Arms:
- **base**;
- **ST** = base + `--combinator-state`;
- **STQ** = ST + `--skeleton-quota 0.1`.

**Phase B** (run only if phase A passes): all 49 problems, 10 fresh
matched seeds (18000-18009), base vs STQ.

**Predictions.**
1. STQ >= base - 3 (of 30) on all three phase A problems.
2. ST < base on graph_reachability, replicating the earlier drop. This is
   not part of the decision rule.
3. Phase B, STQ vs base:
   - STQ >= base - 1 on every problem;
   - STQ >= base + 7 on each of the ten stateful problems;
   - on problems both arms solve 10/10, mean wall <= 2x base.

**Decision rule, fixed now.**
- If 1 and 3 hold, `--combinator-state` and `--skeleton-quota 0.1` both
  become the default.
- If 1 fails, the quota at 0.1 is refuted for this conflict.
- If 1 holds but 3 fails, both stay opt-in, and the phase B failure
  decides the follow-up.

## 60 (results). Quota refuted, and the regression it targeted is a step-limit artifact

`data/skeleton_quota_ab_20261005/` (not versioned; 01:53-02:18, 270
runs). Verified successes of 30 matched seeds (17000-17029):

| problem | base | ST | STQ |
|---|---|---|---|
| graph_reachability | 17 | 13 | 13 |
| second_smallest | 28 | 27 | 29 |
| min_gap | 11 | 14 | 15 |

- **Prediction 1 failed** on graph_reachability (STQ = base - 4). By the
  rule, the quota at 0.1 is refuted. STQ is identical to ST on that
  problem, so the quota changed nothing there.
- **Prediction 2 held** (13 < 17).
- min_gap: ST is *above* base. Phase B's 6 -> 4 was 10-seed noise.

**Diagnosis, after the run.**
- **Not crowding.** In every graph_reachability run, success or failure,
  base or ST, the champion is the same 69-node relaxation skeleton up to
  variable renaming. No failing final program uses a stateful block.
- **What separates success from failure is the level at which that
  skeleton is first found:**
  - Found at level 2, invariance lookahead checks N = 3..5 and the
    program is confirmed: success.
  - If a small fluke solves level 2 first (in the inspected ST failure, a
    7-node program), the skeleton is found at level 3. Its lookahead must
    then pass N = 6, where its O(n^3) relaxation exceeds
    `--step-limit 5000` on the denser instances. It is marked "breaks
    before N=6" and the run fails.
- **Check:** the same 30 seeds rerun at `--step-limit 20000`
  (`data/graph_steplimit_check_20261005/`, not versioned) give **base
  30/30, ST 30/30, STQ 30/30**. Mean wall is 0.54 s vs 0.61 s.

**Consequences.**
- Every graph_reachability gap in sections 56, 58, 59 and 60 measured
  whether the skeleton showed up at level 2, not whether the search found
  it. Extra draws such as the stateful pieces shift that race. The
  "graph regression" of `--combinator-state` is an artifact of the
  battery's step limit, not a cost of the pieces. section 58's crowding
  mechanism, and the motivation for sections 59 and 60, were wrong.
- The decisions recorded under the fixed rules stand as recorded:
  `--combinator-state`, `--adaptive-families` and `--skeleton-quota` all
  stay opt-in. The quota and the adaptive weights showed no benefit, so
  they are not candidates.
- With graph_reachability at a sufficient step limit, the remaining
  objections to `--combinator-state` as the default are 10-seed noise
  (second_smallest, min_gap, each cleared at 30 seeds) and phase B's
  sub-second 2.11x cost on count_ascending_runs. A new pre-registered
  battery with the corrected step limit is needed before any change.

## 61. `--combinator-state` as the default, with the step limit corrected -- predictions registered before running

Section 60 showed that the graph_reachability gaps of sections 56 and
58-60 were an artifact of `--step-limit 5000`. This repeats section 56's
question A on all 49 problems, at **`--step-limit 20000` for every problem
and both arms**, with 10 fresh matched seeds (19000-19009), section 56's
max-n, walls and budgets, and per-seed `--benchmark`.
Script: `scripts/run_state_default_steplimit.sh`. Arms:
- **base**: current default;
- **ST**: base + `--combinator-state` (share 1/4, as in section 55).

**Predictions** (section 56's, unchanged):
1. No regression: ST >= base - 1 on every problem.
2. Gains: ST >= base + 7 on each of the ten stateful problems.
3. Cost: on problems both arms solve 10/10, ST's mean search wall <= 2x
   base. The rule is kept as it was, although the closest past miss was
   sub-second (2.11x on count_ascending_runs, section 59 phase B).

**Decision rule, fixed now.** If 1, 2 and 3 hold, `--combinator-state`
becomes the default. If any fails, it stays opt-in and the failing problem
is reported. No retest is promised in advance.

## 61 (results). Success predictions hold; the 2x cost rule fails on sub-second problems, so the default does not change

`data/state_default_steplimit_20261005/` (not versioned; 07:11-08:48,
980 runs, 172 wall timeouts, on problems neither arm solves or in base
runs of the stateful problems). Verified successes of 10 matched seeds
(19000-19009), base / ST:

| problem(s) | base | ST |
|---|---|---|
| the ten stateful problems of section 55 | 0 each | 10 each (lis 9) |
| graph_reachability | 10 | 10 |
| count_distinct / second_smallest | 3 / 9 | 7 / 10 |
| count_ascending_runs / max_gap_sorted / min_gap | 10 / 4 / 4 | 9 / 3 / 3 |
| three_sum | 7 | 7 |
| every other solved problem (31) | 10 each | 10 each |
| max_adjacent_gap, cycle, max_profit, count_leaders, has_majority | 0 | 0 |

1. **No regression: held.** The worst case is -1 (count_ascending_runs,
   max_gap_sorted, min_gap). At step limit 20000, graph_reachability is
   10/10 in both arms, confirming section 60.
2. **Gains: held.** +9 or +10 on all ten stateful problems.
3. **Cost: failed.**
   - contains_ordered: 0.42 s -> 1.35 s (3.2x);
   - contains: 0.28 s -> 0.59 s (2.1x).

   Every other shared 10/10 problem is <= 1.6x, and the largest absolute
   increase is under 1 s. Where ST solves a problem base also solves
   slowly, it is much faster: count_distinct 67 s -> 30 s, max_gap_sorted
   96 s -> 10 s.

**Decision: by the fixed rule, `--combinator-state` stays opt-in.** The
rule measures a relative cost, which a sub-second baseline makes easy to
exceed. Whether the rule should be replaced (for example by an absolute
or a battery-total wall budget) is a decision to make and register
*before* the next battery, not a reinterpretation of this one.

## 62. `--combinator-state` becomes the default by explicit decision, after section 61's data

This is a decision made *after* seeing section 61's results, not the
outcome of a pre-registered rule. The record says so explicitly.

Section 61's cost rule (ST's mean wall <= 2x base on problems both arms
solve 10/10) failed only on two sub-second problems:
- contains_ordered: 0.42 s -> 1.35 s;
- contains: 0.28 s -> 0.59 s.

Both success predictions held: no problem dropped by more than 1, and
all ten stateful problems went from 0 to 9-10. The project owner decided
that search time is not a criterion for promotion: **what matters is
reachability and diversity of the problems solved**, and on those
`--combinator-state` is a pure gain.

The CLI default is now `--combinator-state` on.
`--no-combinator-state` restores the previous behavior. GenConfig's own
default stays false, so the generator's byte-identity tests are
unchanged.

Going forward, default-promotion rules for search features should be
written on success and diversity (no regression, gains). Wall time is
reported, not used as a gate.

## 63. Section 22's binary-search ladder, stage 3, with the hole filled by enumeration -- predictions registered before implementing

Section 22 concluded that "there is no gradual tipping point": with
everything hand-written except the final "which way to move" decision
(stage 3), free search found binary search in 1/5 seeds, and that one had
degenerated to O(n) (`hi = hi - 1`). That hole was filled by *mutation*.
Sections 41-44 later showed, on TSP's pruning guard, that the same kind of
hole is reachable by *enumeration*, provided selection has a signal that
separates the right answer from distribution-specific impostors (there,
weight scaling). Section 22 was never retried with enumeration. This
section does that.

**Mechanism (`--fill-bsearch-hole`, a research mode like
`--fill-tsp-guard`).** Section 22's stage-3 shape is fixed, and the hole is
`if (C) { A } else { B }` in the not-found branch:
- **C**: every condition from `enumerateConditions` over the atoms n, target, lo,
  hi, mid, v, dir and the literals 0..2. Terms are an atom, atom +/- atom,
  and, new here, (atom - atom) * atom. This extension is needed because
  the direction-aware comparison `(v - target) * dir < 0` contains a
  product.
- **A, B**: each is one move from
  {lo = mid+1, lo = mid, lo = mid-1, hi = mid, hi = mid-1, hi = mid+1,
  lo = lo+1, hi = hi-1}. The last two are the linear escapes section 22
  found.
- **Pruning:** conditions are deduplicated by truth vector on states
  traced from the two linear-escape programs, which contain no knowledge
  of the answer.
- **Evaluation:** every (class, A, B) program runs on training
  contains_ordered instances with n = 2..12. Candidates that fail any
  training instance are rejected; the correct ones are ranked by total
  interpreter steps on training.
- **Checks on the winner:**
  - held-out correctness on n = 13..20, generated plus adversarial
    instances;
  - step growth on the problem's benchmark instance at n = 16 and n = 128.

**Predictions.**
1. **Reach:** at least one correct class implements genuine bisection
   (steps(128)/steps(16) <= 3; O(log n) gives about 1.75, O(n) about 8).
2. **Contrast:** correct O(n) escapes are also in the correct set.
   Section 22's degeneration is inside the space; it just has to lose on
   selection.
3. **Select (the decisive one):** the training-steps winner is correct on
   held-out n = 13..20 and has steps(128)/steps(16) <= 3.

**Decision rule, fixed now.**
- If 3 holds, section 22's conclusion is refuted for stage 3: the tipping
  point exists, and what was missing was the filling method, not
  hand-written knowledge. The next step is section 22's stage 2 (the
  found-check also a hole).
- If 1 holds but 3 fails, the wall is selection, as in section 41. The
  follow-up then selects on larger n (training up to 20), registered
  before running, as section 41 did.
- If 1 fails, the hole is out of reach of this grammar. That is
  reported, not patched.

## 63 (results). Enumeration reaches binary search from stage 3; selection holds only by a tie-break, and the instance generator has a blind spot

`data/bsearch_hole_20261005/seed42_n12.txt` (not versioned): `./algosearch
--problem contains_ordered --fill-bsearch-hole --max-n 12 --seed 42
--threads 8`, 7.5 min.

**Search space.** 9600 training instances (n = 2..12), 49,740 conditions
collapsing to 7,394 truth-vector classes, times 56 move pairs (plus the
8 condition-free pairs). Every candidate was run.

**Survivors: 16 candidates correct on every training instance.** All 16
use the product term (v - target) * dir. No product-free condition
survives.
- 4 bisect, with step growth x1.49 from n = 16 to 128. This is the
  hand-written `lo = mid + 1` / `hi = mid` pair under four spellings of
  the condition.
- 4 are mixed (`hi = hi - 1` on one side), growth x5.06.
- 8 are linear, growth >= x6.7. This includes section 22's `lo = lo + 1`
  escape.

| prediction | result |
|---|---|
| 1. reach (a bisecting class exists) | **held** (4) |
| 2. contrast (O(n) escapes also correct) | **held** (8 linear, 4 mixed) |
| 3. select (the training-steps winner is correct on held-out n = 13..20, growth <= 3) | **held, with a caveat** |

**The caveat.** The four bisecting candidates tie exactly on training
steps, so the winner was decided by enumeration order. Two of the four
(`(v - target) * dir < lo` and its negation) are unsound: in the
not-found branch they also move right when 0 < diff < lo. They pass all
9600 training instances and all 3904 held-out ones, because
contains_ordered's generator never produces a target just below a probed
value early in the search. A direct probe on dense consecutive haystacks
(0..n-1, every target, both directions, n = 2..40; 1638 instances) has
the sound decision wrong on 0 and the impostor wrong on 794. This is
the same failure class as section 41's TSP guards: a condition valid
only on the generator's distribution, tied with or beating the sound
one. Here it merely tied, so a coin flip of ordering picked the sound
one.

**Reading.**
- **Reach is no longer the wall.** Section 22's "no gradual tipping
  point" was a property of mutation. Enumeration fills the stage-3 hole,
  and selection by steps cleanly separates bisection from the linear and
  mixed escapes (growth x1.49 vs x5-7). By the rule, the conclusion is
  refuted for stage 3, and stage 2 is next.
- **Two honest limits.**
  - The product term (atom - atom) * atom was added because the
    direction-aware answer needs it. That is grammar knowledge supplied
    from outside, smaller than a whole decision but not nothing.
  - Separating sound from distribution-fit decisions needs instances
    the generator does not make.
- **Generator blind spot.** contains_ordered's instances should include
  dense consecutive haystacks. This affects the main search too: any
  search on this problem can accept the impostor. It is fixed in the
  next commit, and section 63 is re-run once with it.

**Re-run with the generator fixed** (`data/bsearch_hole_20261005/seed42_n12_everypos.txt`).

The fix: contains_ordered now adds, at every level, an evenly spaced
haystack (0, 2, 4, ...). The target appears at every position, and is
absent in every gap and at both ends, in both directions. A test checks
that the impostor is rejected.

Same command, 9930 training instances, 8432 classes, 12 survivors:

| | survivors |
|---|---|
| bisecting, growth x1.49 | **2**, both spellings of the hand-written decision |
| mixed, growth x5.06 | 2 |
| linear, growth >= x6.7 | 8 |

Both bisecting winners are correct on all 4448 held-out instances
(n = 13..20). The tie between them is a tie between equivalent programs,
not a coin flip. Section 63's prediction 3 now holds without the caveat.

What remains outside the system is the product term in the enumeration
grammar. The next step, by section 63's rule, is stage 2, where the
found-check is also a hole. The generator change also alters
contains_ordered's training set for ordinary searches; earlier
contains_ordered results were obtained without it.

## 64. Stage 2: the found-check is a hole too -- predictions registered before running

The hole is `if (C1) { F } else { if (C2) { A } else { B } }`. Only the
loop frame, the direction detection and the midpoint stay hand-written.
This is section 22's stage 2, except that F is restricted to two
loop-exiting bodies (below). Mode: `--fill-bsearch-hole --bsearch-stage 2`.
- **C2, A, B:** as in section 63 (8432 decision classes x 56 move pairs).
- **F:** one of {found = 1; hi = lo} and {found = 1; lo = hi}. A body
  without an exit (found = 1 alone) never terminates once the target is
  hit, so it is left out.
- **C1:** the same enumerated grammar as C2, pruned by the instances' own
  labels. States at the found-check are traced from the two linear
  escapes with C1 = 0, and each is labeled by whether that instance's
  target is absent.
  - A C1 true at any absent-instance state is dropped, since it would
    answer 1 where the answer is 0.
  - A C1 never true at any present-instance state is dropped, since it
    could never answer 1.
  - The survivors are deduplicated by truth vector.

  This is pruning by the problem's own oracle, with no knowledge of the
  algorithm. It is still a heuristic: the traced states are those of the
  linear escapes, not of bisection.

**Evaluation change, verified neutral.** Rejection runs now use a
20,000-step limit and a per-thread "killer" instance order. Survivors are
re-evaluated in full at 1,000,000 steps. Re-running stage 3 gives the
identical result: 12 survivors, the same two winners. Wall time went from
6 min to 12 s.

**Predictions.**
1. The hand C1, `v == target`, survives the label pruning.
2. **Reach:** at least one candidate correct on all training
   instances bisects (growth <= 3).
3. **Select:** every candidate tied for the fewest training steps is
   correct on held-out n = 13..20 and bisects.

**Decision rule.**
- If 2 and 3 hold, stage 2 is reached by enumeration.
- Stage 1 (the whole loop body free) is a statement-sequence hole that
  this method does not cover. It would be reported as the next frontier,
  not attempted here.
- If 2 fails, the wall is at stage 2 for this grammar.
- If 2 holds and 3 fails, the wall is selection.

## 64 (results). Stage 2 reached: enumeration rebuilds the found-check and the decision together

`data/bsearch_hole_20261005/stage2_seed42_n12.txt` (not versioned).

**Search space.**
- C1: 49,740 found-check conditions -> 119 consistent with the labels ->
  66 classes, x 2 found bodies.
- C2, A, B: 8432 decision classes x 56 move pairs.

**Survivors: 24 candidates correct on all 9930 training instances.**
Every one has C1 = `v == target`, under either found body.
- 4 bisect (growth x1.49).
- 4 are mixed (x5.06).
- 16 are linear (>= x6.7).

| prediction | result |
|---|---|
| 1. `v == target` survives the label pruning | **held**; it is the only C1 in any survivor |
| 2. reach (a bisecting survivor) | **held** (4) |
| 3. select (all step-tied winners correct on held-out and bisecting) | **held**: all 4 correct on 4448 held-out instances, x1.49 |

The four winners are the hand-written program up to two harmless
choices: how the found branch exits (`hi = lo` or `lo = hi`) and the
spelling of the decision. They tie with it exactly at 911,277 training
steps.

**Reading.** Section 22's ladder found nothing short of the full
algorithm by mutation. By enumeration, both stage 3 and stage 2 are
reached and correctly selected. What remains hand-supplied:
- the loop frame, the direction detection and the midpoint;
- the product term in the condition grammar;
- the restriction of F to two loop-exiting bodies.

Stage 1 (the whole loop body, midpoint included, as a free
statement-sequence hole) is outside this method. Filling it would need
enumeration of statement sequences, which is the next frontier, not
attempted here.

## 65. How much of each indispensable skeleton can enumeration rebuild? Single-hole ablation -- predictions registered before implementing

Sections 63-64 rebuilt two parts of binary search by enumeration. This
section asks the same of every skeleton that section 56 found
indispensable, one hole at a time, to measure how much human knowledge
each one really carries.

**Mode `--ablate-skeleton NAME`** (research mode, like `--fill-tsp-guard`).
1. **Reference program.** Generate random programs with
   `ALGOSEARCH_FORCE_SKELETON=NAME`, loop bias 1 and the combinator off,
   until one solves every training instance. That program is the
   reference.
2. **Sites.** Every If/While condition and every right-hand side of a
   scalar or array assignment in the reference program. Each site
   becomes a hole on its own; the rest of the program stays as is.
3. **Fill grammar.** Atoms are the program's scalars, the literals 0..2,
   `arr[v]` for each scalar v, and `arr[0..2]`. The last group was
   added after a pilot on gcd showed its `arr[0]`/`arr[1]` loads were
   outside the grammar; that was a grammar-coverage fix, made before
   any battery.
   - Value holes: an atom, or `a op b` with op in + - * / %.
   - Condition holes: lhs an atom or atom +/- atom, rhs an atom, op in
     < > == <= >= !=.

   No hole-specific terms (section 63's product is not included).
4. **Dedupe** by value/truth vector on states traced at the site while
   running the reference program. This is a pruning shortcut, disclosed:
   the states are the reference's. Then run every class with fast
   rejection, and rank survivors by training steps.
5. **Classify each site:**
   - **recovered**: every candidate tied for the fewest steps passes
     held-out;
   - **reach-only**: some training-correct candidate passes held-out,
     but a tied winner does not;
   - **unreached**: no candidate passes held-out.

   The log also records whether the original expression is in the
   grammar.

**Skeleton -> problem, with training max-n (held-out: the next 3 levels
plus adversarial instances):**
- recurrence -> gcd 20, fibonacci 20;
- nesteddivisor and nesteddivisornative -> is_prime 20;
- repeatedmultiply -> factorial 15, power 10;
- average -> average 10;
- threesum -> three_sum 8;
- graphreachability -> graph_reachability 6;
- selectionsort -> sorting 5.

**Predictions.**
1. Of the sites whose original expression is in the grammar, >= 80% are
   reached (recovered or reach-only).
2. Of the reached sites, >= 70% are recovered: step-based selection
   works, as in sections 63-64.
3. Every skeleton has at least one recovered site.
4. Unrecovered sites are proportionally more common among value holes
   than among condition holes.

**Decision rule.** This is a measurement, so no default changes. Sites
recovered across all of a skeleton's problems become candidates for
"skeleton with enumerated holes" in an integration experiment, to be
registered separately.

## 65 (results). 100 of 112 skeleton sites are rebuilt by enumeration; what resists is compound array indexing, three-term sums, and selection on graph_reachability

`data/skeleton_ablation_20261006/` (not versioned;
`scripts/run_skeleton_ablation.sh`, seed 1, about 2 min for all ten
pairs).

| skeleton -> problem | sites | in grammar | recovered | reach-only | unreached |
|---|---|---|---|---|---|
| recurrence -> gcd | 9 | 9 | 9 | 0 | 0 |
| recurrence -> fibonacci | 8 | 8 | 8 | 0 | 0 |
| nesteddivisor -> is_prime | 14 | 12 | 14 | 0 | 0 |
| nesteddivisornative -> is_prime | 9 | 7 | 8 | 0 | 1 |
| repeatedmultiply -> factorial | 11 | 10 | 11 | 0 | 0 |
| repeatedmultiply -> power | 12 | 12 | 12 | 0 | 0 |
| average -> average | 11 | 11 | 11 | 0 | 0 |
| threesum -> three_sum | 13 | 12 | 12 | 0 | 1 |
| graphreachability -> graph_reachability | 16 | 14 | 6 | **8** | 2 |
| selectionsort -> sorting | 9 | 9 | 9 | 0 | 0 |
| **total** | **112** | **104** | **100** | **8** | **4** |

**Predictions.**
1. **Held:** 104 of 104 in-grammar sites reached (>= 80% predicted).
2. **Held:** 100 of 108 reached sites recovered, 93% (>= 70% predicted).
3. **Held:** every skeleton has >= 1 recovered site (graphreachability
   has 6).
4. **Failed, the opposite way.** Condition holes are the harder kind: 7
   of 28 unrecovered (25%) against 5 of 84 value holes (6%).

**What resists.**
- **Unreached (4 sites), all outside the grammar:**
  - graph's `arr[1 + v7] == 1` and `arr[v1 + v7*v4 + v5] == 1` (array
    reads at compound indices);
  - three_sum's `arr[i] + arr[j] + arr[k] == 0` (three-term sum);
  - nesteddivisornative's `v4 % v1 == 0` (no `%` in condition terms).

  These are the irreducible pieces under this grammar. Each is a
  compound expression, not a decision.
- **Reach-only (8 sites), all in graphreachability:**
  - A correct fill exists in each case, but the training-steps winner is
    wrong on V = 7..9.
  - The winners do less work, with fewer relaxation rounds or shortcuts
    such as `v5 == arr[v4]`, which is exact at V <= 6 and not beyond.
  - This is section 41's selection wall again: sparse small instances
    reward under-computation.
  - It is also the skeleton whose training set is smallest (86
    instances).

**Caveats.**
- **"Recovered" means correct, not complexity-preserving.**
  nesteddivisor's `v1 * v1 <= v4` bound (O(sqrt n)) is outside the
  condition grammar, which has no products, and was replaced by
  `v1 + v1 <= v4` (O(n)). That fill is correct, and it is the fastest
  one *available*. The opposite also happened: gcd's subtraction step
  `v2 - v4` was replaced by `v1 % v4`, turning subtractive Euclid into
  the faster modulo form.
- **Single holes only.** The rest of the program always stays intact;
  multiple simultaneous holes were not tested.
- Dedupe used states traced from the reference program, and each pair
  used one reference program (seed 1).

**Reading.** Measured site by site, the hand-written knowledge in nine of
the ten skeleton/problem pairs is almost entirely their *shape*: loop
nesting, which variable gets which role, and where the answer is
written. Given the shape, enumeration rebuilds nearly every individual
condition and value. The exceptions are compound index arithmetic, a
three-term sum, and selection on a skeleton whose small instances cannot
tell full work from shortcuts.

**Next, per the rule:** an integration experiment (registered
separately), in which skeletons whose sites are all recovered are
offered with their recovered sites as enumerated holes.

## 66. Shape-only skeletons + enumerative repair inside the search -- predictions registered before implementing

Section 65 showed that, given a skeleton's *shape*, enumeration rebuilds
nearly every condition and value, one hole at a time and with the rest of
the program correct. Inside the search, a skeleton offered as shape only
has *all* its sites wrong at once. So the question is whether the
search's partial score can guide repair site after site: coordinate
descent, with each step an exhaustive single-site enumeration.

**Two mechanisms.**
- **`--skeleton-holes` (generation).** When a hand-written skeleton is
  drawn, every site whose expression lies in section 65's hole grammar is
  replaced by a random expression from that grammar, over the block's own
  scalars. Sites outside the grammar (compound indices, three-term sums)
  stay as written, so the shape plus those pieces is what the skeleton
  still contributes. The grammar test is syntactic and automatic, with no
  per-skeleton list.
- **`--hole-repair K` (search operator).** At each size step, for each
  of the top K beam survivors (any program, skeleton or not):
  1. pick one site uniformly at random;
  2. enumerate the hole grammar over the program's scalars;
  3. deduplicate on the states traced at that site on the current
     level's instances;
  4. score every class on those instances (solved count, then steps) and
     add the best one to the candidate pool.

  The pool is scored normally, so the beam decides.

**Experiment.** Problems: gcd, fibonacci, is_prime, factorial, power,
average, three_sum, sorting (all section 65 sites recovered or out of
grammar), plus graph_reachability (8 reach-only sites; expected to fail).
10 matched seeds (20000-20009), section 56's max-n, walls and budgets,
`--step-limit 20000`, per-seed `--benchmark`. Arms:
- **base**: current default (full skeletons);
- **HR**: `--skeleton-holes --hole-repair 10`;
- **H**: `--skeleton-holes` alone (control: shape without repair).

**Predictions.**
1. H <= 2/10 on at least 6 of the 8 main problems: scrambled shapes are
   not solved by mutation alone (sections 18, 22).
2. HR >= 7/10 on at least 5 of the 8 main problems: repair turns shape
   into solutions.
3. graph_reachability: HR < base (section 65's selection wall).

**Decision rule.**
- If 1 and 2 hold, the result reads "shape + enumerative repair
  suffices" for those problems, and the hand-written fills of those
  skeletons are candidates for removal (a later, separately registered
  step).
- If 2 fails, single-site coordinate descent does not compose under the
  search's score.
- Nothing becomes default from this section alone.

## 66 (results). Shape alone does not compose: single-site repair from a scrambled skeleton reaches nothing; battery not run

The registered battery was **not run**: pilots showed the mechanism fails
at its base, and the result does not need ten seeds to read. What was run:

1. **Pilot 1, as registered** (`--hole-repair` on the top beam
   survivors). gcd, seed 20000: no solution. The beam is dominated by a
   non-skeleton 72/100 plateau program, and repair only ever sees that.
   This is a design defect, found before any battery.
2. **Pilot 2, amended design.** Repair targets skeleton-tagged survivors,
   topped up with fresh scrambled skeleton draws. Each target is
   hill-climbed site by site until a pass without improvement. gcd seed
   20000: no solution in 249 s.
3. **Direct probe of the core question** (`scripts/probe_shape_climb.cpp`,
   linked against the library). Twenty scrambled skeletons per problem
   were hill-climbed with full class lists, no cap and up to 6 passes, on
   the training levels:

   | problem (skeleton) | fully solved | typical trajectory |
   |---|---|---|
   | gcd (recurrence) | **0/20** | 15 stay at 0; 5 climb to a 550/857 plateau |
   | factorial (repeatedmultiply) | **0/20** | all stay at 0 |

**Why.** Section 65's one-hole result holds when every *other* site is
already right. From an all-scrambled start, no single site change moves
the score at all: the program crashes or loops, or the output is wrong
on every instance until several sites are right together. Coordinate
descent has no gradient to follow. This is the same "no gradient"
diagnosis as section 18, now one level up: per-site enumeration solved
the gradient problem *within* a site, not *across* sites.

**Predictions.** 2 (HR >= 7/10 on >= 5 problems) is refuted by the
mechanism probe. 1 and 3 were not measured. By the rule, *single-site
coordinate descent does not compose under the search's score.* The
hand-written fills stay; nothing is removed. `--skeleton-holes` and
`--hole-repair` remain as opt-in research flags (off, byte-identical).

**What it says about the skeletons.** Section 65 measured what is
recoverable *individually*. Section 66 shows the fills are not
recoverable *jointly* by local search. The knowledge in a skeleton is its
shape *plus* the coordination between its sites. Candidate next
approaches, none tried yet:
- enumerate pairs or triples of sites jointly, guided by section 65's
  per-site class counts;
- a constraint or SMT-style solver over the sites (still no ML);
- fill sites in dependency order (loop guard after loop body), each
  against intermediate-state labels rather than final output.

## 67. Joint enumeration of site pairs -- probe, prediction registered before running

This is section 66's first candidate fix. The climb from a scrambled
skeleton stays the same as section 66's probe, with one change: when no
single-site fill improves the score, the climb tries every *pair* of
sites (i, j). It enumerates fill_i x fill_j, each from the site's classes
deduplicated on the current program's states, and accepts the first pair
that strictly improves (solved count, then steps). Single-site moves
resume after any accepted move.

Cost control:
- scoring runs on a sample of 40 training instances;
- a candidate stops as soon as it can no longer beat the current
  program;
- the full training set re-checks the final program.

Probe: `scripts/probe_shape_climb.cpp pairs`, 20 scrambled skeletons
each for gcd (recurrence) and factorial (repeatedmultiply), the same
draws as section 66 (seeds 100-119).

**Prediction:** pairs fully solve >= 5/20 on each problem (section 66:
0/20 and 0/20).

**Rule:**
- If it holds, a registered in-search battery follows.
- If it fails, triples are not tried by brute force (cost x 100+). The
  dependency-order idea is next.

## 67 (results). Pairs move some climbs off zero but none reach a solution: 0/20 and 0/20

`data/pair_climb_20261006/` (not versioned), a few minutes in total.

| problem | stuck at 0 | stopped at a plateau | fully solved |
|---|---|---|---|
| gcd (857 instances) | 10 | 10 at 550/857 | **0/20** |
| factorial (66 instances) | 11 | 9 at 12-18/66 | **0/20** |

Compared with section 66, pairs lift more climbs off zero (gcd: 5 -> 10
reach the plateau). Every gcd climb that leaves zero still stops at the
same 550/857 local optimum. Half the draws stay at 0 even with pairs: no
single site and no pair of sites gives them any signal.

**Prediction (>= 5/20 each): failed.** Following the rule, triples are
not tried by brute force. The obstacle is twofold: draws where three or
more sites must be right before anything scores, and a deceptive
plateau (550/857) that pair moves cannot leave. The next idea on the list
is filling sites in dependency order against intermediate-state labels.
That changes the signal rather than the neighborhood size, so it is the
right kind of next attempt.

## 68. Filling all of a skeleton's holes at once with an SMT solver (sketching / CEGIS) -- pilot, predictions registered before implementing

Sections 66-67 failed because local search needs a gradient, and a
scrambled skeleton gives none until several sites are right together. A
constraint solver needs no gradient: it looks for one assignment of
*every* hole that makes every given instance correct. This is the
classic program-sketching setup (Solar-Lezama's Sketch, solved by
counterexample-guided inductive synthesis, CEGIS). It is deduction, not
learning, so the no-ML rule holds. The tool is Z3, from the `z3-solver`
Python package in a scratch virtualenv. Nothing is added to the project's
build.

**Pilot pieces.**
- `scripts/sketch_bridge.cpp` (C++, linked against the library) has two
  modes.
  - `export` draws section 66's scrambled skeleton (same draws: seeds
    100-119) and writes JSON with three things: the AST with every
    in-grammar site marked as a hole, the atom list, and the instances.
  - `verify` reads the solver's fills, rebuilds the program, and checks
    it with the real interpreter and `isSolution` on the training levels
    (<= 6) and on held-out (the next 3 levels plus adversarial
    instances).
- `scripts/sketch_solve.py` encodes the program symbolically:
  - 32-bit bitvectors with C semantics (truncating division; division
    by zero is an error);
  - array reads as if-then-else chains;
  - if/while merged by if-then-else;
  - while loops unrolled up to U = 16 iterations, with "still running
    after U" counted as failure.

  Each hole is a choice of form x atoms x operator over section 65's
  grammar, the same one, with nothing added. CEGIS runs over the
  training instances whose input values are all <= 12: solve, run the
  candidate concretely on that pool, add the first instance it fails,
  and repeat.

Problems: gcd (recurrence) and factorial (repeatedmultiply), 20 draws
each.

**Predictions.**
1. **Tractable:** every draw gets SAT or UNSAT within 10 minutes.
2. **Correct:** >= 80% of SAT draws give a fill that `verify` confirms
   on full training and held-out.
3. **Diagnostic, not a pass/fail prediction.** The split of draws into
   SAT and UNSAT tells whether section 66-67's 0/20 came partly from
   shapes that cannot work at all, or from local search missing
   solutions that exist.

**Rule.** If 1 and 2 hold, solver-based hole filling is viable, and an
in-search integration follows (registered separately). If 1 fails, the
encoding does not scale even to these skeletons. If 2 fails, the
small-value training pool admits impostors, and CEGIS needs larger or
adversarial instances.

## 68 (results). The SMT encoding does not scale even to gcd's 7-hole sketch: 16/16 timeouts; pilot stopped for memory

`data/sketch_pilot_20261006/` (not versioned; `scripts/run_sketch_pilot.sh`
with 8 parallel jobs, 600 s each, unroll 8, solver width 8, pool values
<= 10).

The pilot was stopped by hand after 16 gcd draws. Eight concurrent Z3
processes had pushed the machine to 14 of 15 GB, and the session's
background monitor was killed for low memory. The factorial draws were
never reached. They are not reported.

| gcd draws finished | SAT and verified | UNSAT | timeout (600 s) |
|---|---|---|---|
| 16 of 20 | 0 | 0 | **16** |

- Most draws timed out after 2-4 CEGIS rounds (5-7 instances). Each
  added counterexample made the next query much harder.
- Draw 115 is different: 112 rounds in 600 s. Z3 kept returning fills
  that satisfied the encoding but failed the concrete check. The likely
  cause is a gap between the solver's 8-bit wraparound and the concrete
  32-bit run. This is an encoding artifact and should be read as such.
- Earlier single-draw attempts (gcd 100) agree:
  - at 32 bits, unroll 16: still in round 2 at 600 s;
  - at 12 bits, unroll 12: round 2 at 500 s;
  - with a QF_BV solver and bitvector selectors at 8 bits: round 4 at
    500 s.

**Prediction 1 (tractable within 10 minutes): failed** on every gcd draw
measured. Prediction 2 could not be evaluated (no SAT). Diagnostic 3 is
open: no draw was proven UNSAT either.

**Reading.** This is a negative result about *this encoding*, not about
sketching in general. The encoding is naive in three ways:
- every hole carries the whole grammar as nested if-then-else over all
  atoms and forms;
- the grammar includes division and modulo, which bit-blast badly;
- the encoding is repeated in every unrolled iteration of every
  instance.

Real sketch solvers avoid exactly this. They use a separate
interpretation per hole occurrence, choose hole values once and share
them, and keep many small queries instead of one growing one. Narrowing
the grammar per hole, or fixing each hole's form first and then solving
only for atoms, would also shrink the problem. Those are engineering
steps of a different size than this pilot.

By the rule, the encoding does not scale even to these skeletons.
Section 66's conclusion stands for now: the fills' coordination is real
knowledge that neither local search nor this naive solver recovers.

## 69. Closing the "shape vs fills" thread (sections 59-68), and the code clean-up

**What the thread established.**
1. **The graph_reachability regression was an artifact** (sections
   58-61). Every gap measured at `--step-limit 5000` was a race to find
   the skeleton at level 2, because the O(n^3) relaxation exceeds that
   limit at N = 6 lookahead. At 20,000 there is no gap. As a result,
   `--combinator-state` is the default since section 62. The two
   mechanisms built to fix the supposed crowding were refuted on their
   own terms: adaptive family weights (59) and the protected skeleton
   quota (60).
2. **Holes inside a correct shape are reachable by enumeration.**
   - Section 22's binary-search ladder, which mutation could not climb,
     is climbed at stages 3 and 2 by exhaustive enumeration with
     step-based selection (63-64).
   - Single-hole ablation of the ten indispensable skeleton/problem pairs
     rebuilds 100 of 112 sites (65).
   - The exceptions are compound array indices, a three-term sum, `%`
     inside a condition (all outside the grammar), and selection on
     graph_reachability, whose small instances reward under-computation.
   - Along the way, contains_ordered's generator got the target at
     every position. Its old instances admitted an unsound
     binary-search decision.
3. **The coordination between sites is not recovered.** Starting from a
   scrambled skeleton:
   - single-site repair: 0/20 (66);
   - pair-site repair: 0/20 (67);
   - a naive SMT sketch encoding: 16/16 gcd timeouts (68).

   The knowledge in a skeleton is its shape *plus* the coordination of
   its fills; each fill alone is cheap. Closing this gap would take
   engineering of a different size: a real sketch solver encoding, or
   intermediate-state labels. It is left open, deliberately.

**Code clean-up (this commit).** Following the merge policy (refuted
opt-in features are not kept because they are harmless), these are
removed:
- adaptive family weights and the per-candidate family tags;
- `--combinator-state-share`;
- `--skeleton-quota`;
- `--hole-repair` and the `--skeleton-holes` CLI flag.

`BeamSearch.{hpp,cpp}` and `tests/combinator_tests.cpp` are back to their
pre-section-59 versions. The generator's stateful split and skeleton roll
are back to their section 55-58 form.

Kept:
- `--fill-bsearch-hole [--bsearch-stage 2|3]` and `--ablate-skeleton`
  (positive results, sections 63-65);
- `search/HoleGrammar.{hpp,cpp}`;
- `GenConfig::skeletonHoles` (no CLI flag), which the section 66-68
  probes use;
- the stage-2 builder;
- the interpreter's assignment tracing;
- the contains_ordered fix;
- the non-benchmark `--combinator-state` fix from section 59.

The three battery scripts that need removed flags say which commit to
build. `make test`: 143/143.

## 70. Phase-1 reachability for the five never-solved problems -- prediction registered before running

The five problems are max_adjacent_gap, cycle, max_profit, count_leaders
and has_majority. They are 0/10 in every arm of sections 56, 59 and 61.

`scripts/probe_reachability.cpp` samples 20,000 programs per problem
from the *default* generator family (combinator 0.5 with step axes and
stateful pieces, loop bias 0.4, every remaining skeleton; numVars 9).
Each sample is checked in three shapes:
- **one**: a single drawn block;
- **one+T**: that block plus section 51's enumerated tail;
- **two+T**: two independent blocks in sequence, then the tail.

For each shape it counts the samples correct on every training
instance (levels <= 8; cycle <= 5) and how many of those also pass
held-out (the next 3 levels plus adversarial instances).

**Prediction:** zero generalizing samples on all five problems in all
three shapes. Their failure is then expressivity (the family lacks a
piece), not search.

**Rule:**
- A problem with a generalizing sample is a *search* failure, and gets
  its own follow-up.
- A problem without one gets a diagnosis of the missing piece from its
  best near miss.

## 70 (results). All five are expressivity gaps: no sample of the default family solves any of them

`data/reachability_unsolved_20261006/` (not versioned). 20,000 samples
per problem, about 2.5 min each.

| problem | one | one+T | two+T | best near miss (training) |
|---|---|---|---|---|
| max_adjacent_gap | 0 | 0 | 0 | 355/536 |
| max_profit | 0 | 0 | 0 | 384/536 |
| count_leaders | 0 | 0 | 0 | 405/536 |
| has_majority | 0 | 0 | 0 | 442/536 |
| cycle | 0 | 0 | 2 correct on training, **0** on held-out | 38/38 (fit) |

**Prediction: held.** No generalizing sample anywhere: these five are not
search failures, the default family does not contain them. The best near
misses show what is missing:

- **max_adjacent_gap: absolute value.** The best near miss is the step
  axis's `BestStep` maximum of `arr[i] - arr[i-1]`. That is right
  whenever the largest jump goes up, and wrong when it goes down. The
  missing piece is an `|.|` combine on the step, or max(maxStep,
  -minStep). Nothing in the family takes the absolute value of a
  difference.
- **max_profit: best difference against all earlier elements.** The
  near miss is again the adjacent `BestStep`. The answer needs
  max over i < j of `arr[j] - arr[i]`: the step relation over *all*
  earlier elements, either as a running minimum combined with a
  difference or as a triangular pair loop with a best-difference
  relation. Neither exists. `BestStep` is adjacent-only, and the pair
  relations count or test, they do not take a best difference.
- **count_leaders: a count over a per-element "for all later
  elements".** The near miss is an unrelated LIS-style DP that agrees on
  405 instances by coincidence. The answer is "count i such that every
  j > i has `arr[j] < arr[i]`" (a quantifier nested inside a count), or
  a right-to-left scan with a running maximum. The family has neither
  nested quantifiers nor a reversed single-pass direction with state.
- **has_majority: a per-element count compared with n/2.** The near
  miss is a palindrome-shaped scan. The answer is "exists i such that
  count of j with `arr[j] == arr[i]` exceeds n/2" (a count nested inside
  an exists, with a threshold on `v0 / 2`), or Boyer-Moore voting. The
  same nesting gap as count_leaders, plus a threshold relative to n.
- **cycle: pointer chasing.** The near miss is an ordering scan. The
  answer must follow `x = arr[1 + x]` (and Floyd's version moves two
  such pointers at different speeds). No family has a loop whose next
  index comes from the data. The two training-correct two+T samples are
  fits that fail held-out, as section 25 anticipated.

**Pattern.** Three of the five (max_profit, count_leaders, has_majority)
need the same missing capability: an aggregate *per element over the
others*, fed into an outer aggregate. These are nested quantifiers or
aggregates over a triangular or pair loop. That is one candidate
combinator axis value, not three skeletons. max_adjacent_gap needs a
one-operator addition (`|.|`). cycle needs a new loop kind (pointer
following).

## 71. Nested aggregates and an absolute step -- design registered before implementing

Section 70 found that max_profit, count_leaders and has_majority all lack
the same capability: an aggregate *per element over the others*, fed into
an outer aggregate. max_adjacent_gap lacks an absolute value on the step.
Both become combinator axis values under one opt-in flag,
`--combinator-nested`. With the flag off, the generator is byte-identical.

**Nested relation.** It applies on the nested loops (full, or triangular
with j > i) of the whole-array and length-prefixed layouts. For each outer
element `elem(i)`, the inner loop over j computes t_i, which is reset per
outer element. The inner aggregate is one of three:
- `CountCmp`: t += 1 when cmp(elem(j), elem(i));
- `ForallCmp`: t starts at 1 and is cleared when cmp(elem(j), elem(i));
- `BestDiff`: t = max of `elem(j) - elem(i)`, starting at 0.

The outer aggregate over t_i is one of:
- `Best`: acc = max t_i, starting at 0;
- `CountIf`: acc += 1 when t_i cmp2 thr;
- `ExistsIf`: acc = 1 when t_i cmp2 thr.

thr is one of 0, 1, or `bound / 2` (n/2). cmp and cmp2 are drawn from
the six comparators.
- On a nested loop, the relation draw gives Nested 2 times in 7, with
  the flag on.
- The difference direction is fixed (j minus i), deliberately. That makes
  one boundary falsifiable below.

**Absolute step.** With the flag on, `BestStep` takes `|elem(i) -
elem(i-1)|` one time in three. It applies to `BestStep` only, not to sums,
also deliberately.

**Protocol.**
1. Implement and freeze (commit).
2. Write post-freeze problems and register predictions.
3. Phase 1: reachability probe with the flag on (`probe_reachability`
   gains the flag) for the four targets and the post-freeze problems.
4. Phase 2: a matched A/B (base vs base + `--combinator-nested`) on all
   registered array/scalar problems, 10 seeds, per-seed `--benchmark`,
   `--step-limit 20000`.

**Decision rule, fixed now.** Per section 62, time is not a gate. The flag
becomes the default if:
- (a) at least 3 of the 4 targets reach >= base + 7 in phase 2;
- (b) no problem drops by more than 1;
- (c) every post-freeze problem predicted reachable has a generalizing
  sample in phase 1.

cycle (pointer chasing) is out of scope here. It needs a new loop kind
and is listed as the next gap.

### 71, post-freeze problems and phase 1 predictions (registered after freeze 5f17009, before running)

Five problems were written after the freeze. Each has labels and a
verified reference program in `tests/composition_problems_tests.cpp`.
- **Predicted reachable** with the nested pieces:
  - `max_frequency`: occurrences of the most frequent value (full loop,
    count ==, outer best);
  - `count_unique`: elements whose value occurs exactly once (full loop,
    count ==, outer count-if t == 1);
  - `count_dominated`: elements with a strictly greater element later
    (triangular, forall-cleared, outer count-if t == 0).
- **Predicted unreachable** (the falsifiable boundaries of the frozen
  design):
  - `max_drawdown`: max(0, max over i < j of arr[i] - arr[j]). The
    difference direction is fixed to j minus i.
  - `total_variation`: the sum of |arr[i] - arr[i-1]|. The absolute
    value exists only inside `BestStep`, not in sums.

**Phase 1 predictions.** `probe_reachability ... nested`: 20,000 samples,
generalizing = correct on training and held-out, in any of the shapes
one, one+T or two+T.
1. All four targets (max_adjacent_gap, max_profit, count_leaders,
   has_majority) have >= 1 generalizing sample.
2. The three predicted-reachable post-freeze problems have >= 1 each.
3. max_drawdown and total_variation have 0.

### 71, phase 1 results

`data/reachability_nested_20261007/` (not versioned). 20,000 samples per
problem, seed 1. Generalizing samples per shape (one / one+T / two+T):

| problem | role | generalizing (one / one+T / two+T) |
|---|---|---|
| max_adjacent_gap | target | 6 / 6 / 3 |
| max_profit | target | 4 / 4 / 4 |
| count_leaders | target | 1 / 1 / 1 |
| has_majority | target | 2 / 2 / 0 |
| max_frequency | post-freeze, reachable | 11 / 11 / 10 |
| count_dominated | post-freeze, reachable | 4 / 4 / 2 |
| count_unique | post-freeze, reachable | **0 / 0 / 0** |
| max_drawdown | post-freeze, unreachable | 0 / 0 / 0 |
| total_variation | post-freeze, unreachable | 0 / 0 / 0 |

- Prediction 1 **held**: all four targets were 0 in section 70 and are
  now reached.
- Prediction 3 **held**: both boundaries stay unreachable.
- Prediction 2 **failed on count_unique**. A diagnostic run after the
  fact (8 more seeds x 40,000 samples, 320,000 in all) still gave 0.
- The diagnostic does not change the rule. A direct count over 400,000
  pure-combinator blocks (combinator bias 1, no skeletons) found 47,466
  blocks with the inner `==` count, 13,304 of them with an outer
  `t == 1`, and **2 that solve count_unique**. Here is why most such
  blocks still fail: the full loop must be paired with the
  whole-array layout, the inner comparator must be `==`, and the outer
  test must be `== 1` or `<= 1`.

So count_unique is *in* the family, but about 5 in a million blocks
build it. That is too rare for a 20,000-sample probe, and probably for
the search as well. Rule (c) is therefore **not met**, and the flag
cannot become the default from this section, whatever phase 2 shows.
Phase 2 still runs, for rules (a) and (b), and to see whether search
reaches count_unique through mutation where sampling did not.

### 71, phase 2 results

`data/nested_ab_20261007/` (not versioned; 07:17-08:25; 1080 runs, 107
wall timeouts, on problems neither arm solves). Verified successes of
10 matched seeds (21000-21009), base / NE (`--combinator-nested`). Rows
not listed are equal in both arms.

| problem | base | NE |
|---|---|---|
| max_adjacent_gap (target) | 0 | **10** |
| max_profit (target) | 0 | **10** |
| count_leaders (target) | 0 | **10** |
| has_majority (target) | 0 | **8** |
| count_dominated (post-freeze) | 1 | **10** |
| count_unique (post-freeze) | 0 | **6** |
| max_frequency (post-freeze) | 10 | 10 |
| max_drawdown / total_variation (boundaries) | 0 / 0 | 0 / 0 |
| min_gap | 2 | 8 |
| count_distinct | 7 | 10 |
| second_smallest | 10 | 9 |

**Rules.**
- (a) **Held**, 4 of 4 targets at base + 8 or more.
- (b) **Held**: the worst drop is -1 (second_smallest).
- (c) **Failed** in phase 1 (count_unique too rare to sample), so by the
  fixed rule the flag stays opt-in.

The search itself reached count_unique in 6 of 10 seeds. Mutation and
selection get to what sampling did not. The two designed boundaries stay
at 0, and two problems the pieces were not aimed at gained (min_gap +6,
count_distinct +3). Promoting the flag despite rule (c) is a decision for
the project owner, as in section 62, and is not made here.

## 72. `--combinator-nested` becomes the default by explicit decision, after section 71's data

This decision was made *after* seeing section 71's results, as an
exception to the fixed rule, and is recorded as such, like section 62.

Rules (a) (all four targets +8 or more) and (b) (worst drop -1) held. Rule
(c) failed: count_unique did not appear in 20,000 phase-1 samples. The
project owner promoted the flag anyway, for these reasons:
- Rule (c) measured random sampling, not the search. The search itself
  solved count_unique in 6 of 10 seeds, which is the property that
  matters (reachability).
- On reachability and diversity, the criterion from section 62, the
  result is a pure gain: six problems go from 0-1 to 6-10, two more
  improve, and nothing drops beyond 10-seed noise.
- Evidence against overfitting: three post-freeze problems reached, two
  designed boundaries held at 0.

Caveat recorded: second_smallest's -1 at 10 seeds is probably noise
(it moved the same way in sections 56 and 58), but it is not proven.

**Rule change for future pieces.** The reachability requirement for
post-freeze problems is judged on phase 2 (the search reaches it in >= 1
seed), not on phase-1 sampling. Phase 1 remains a diagnostic.

The CLI default is now `--combinator-nested` on;
`--no-combinator-nested` restores the old behavior. GenConfig's own
default stays false, so the byte-identity tests are unchanged.

## 73. Canonical outer tests for the nested aggregate -- predictions registered before implementing

Section 71's outer `CountIf` / `ExistsIf` test draws a comparator (6) and
a threshold (0, 1, n/2) independently: 18 combinations. The aggregate t
is never negative: counts, forall flags and best differences from 0 all
satisfy t >= 0. On that domain the 18 collapse:
- `t < 0` is never true;
- `t >= 0` is always true;
- `t > 0`, `t >= 1` and `t != 0` are the same test;
- the other combinations repeat in the same way.

`--nested-canonical-tests` replaces the joint draw with six distinct
tests: `t == 0`, `t > 0`, `t == 1`, `t > 1`, `t > n/2`, `t <= n/2`.
The change is problem-agnostic. It removes equivalent and constant draws
and gives each distinct test about 3x its current share. count_unique,
with its `t == 1`, is only one of the beneficiaries. With the flag off,
nothing changes.

**A/B.** The current default (nested on) vs + `--nested-canonical-tests`.
All 54 array/scalar problems, 10 fresh matched seeds (22000-22009), the
same budgets as section 71, `--step-limit 20000`, per-seed `--benchmark`.

**Predictions.**
1. count_unique >= base + 2 (base expected around 6/10).
2. No problem drops by more than 1.

**Decision rule, fixed now.** Time is not a gate (section 62), and
post-freeze reachability is judged by the search (section 72). If both
predictions hold, the flag becomes the default. If prediction 1 fails,
the canonical tests stay opt-in, and count_unique stays at what the
generic piece gives.

## 73 (results). count_unique 8 -> 10, but has_majority 8 -> 6: the no-regression rule fails by one

`data/canonical_tests_ab_20261007/` (not versioned; 19:06-19:56; 1080
runs, 53 wall timeouts, on problems neither arm solves). Verified
successes of 10 matched seeds (22000-22009), base (nested on) / CT. Rows
not listed are equal in both arms.

| problem | base | CT |
|---|---|---|
| count_unique | 8 | **10** |
| has_majority | 8 | **6** |
| second_smallest / min_gap / max_gap_sorted | 8 / 6 / 8 | 9 / 7 / 9 |
| count_leaders, count_dominated, max_frequency, max_profit, max_adjacent_gap | 10 each | 10 each |

- Prediction 1 **held**, exactly: count_unique gained 2.
- Prediction 2 **failed**: has_majority dropped by 2.
- **Decision: by the fixed rule, `--nested-canonical-tests` stays
  opt-in.**

The drop runs against the mechanism. has_majority's test `t > n/2` has
a 1/6 share under CT against 1/18 before, so if anything the change
should help it. That points to 10-seed noise. The base arm also shows
the noise: count_unique is 8/10 here and was 6/10 in section 71 under
the same configuration. The rule was fixed, though, and a 30-seed retest
of has_majority and count_unique (as sections 54 and 58 did) would
settle it.

## 74. Canonical tests: 30-seed retest of has_majority and count_unique -- predictions registered before running

Following sections 54 and 58: base (nested on) vs CT, with 30 fresh
matched seeds (23000-23029), section 73's budgets and per-seed
`--benchmark`. Script: `scripts/run_canonical_retest_ab.sh`.

**Predictions.**
1. has_majority: CT >= base - 3 (of 30).
2. count_unique: CT >= base + 3 (of 30).

**Decision rule, fixed now.** If both hold, `--nested-canonical-tests`
becomes the default (section 73's other rows already met no-regression).
Otherwise it stays opt-in.

## 74 (results). Not noise: canonical tests cost has_majority 29 -> 22 of 30; they stay opt-in

`data/canonical_retest_20261007/` (not versioned; 19:59-20:19; 120 runs).
Verified successes of 30 matched seeds (23000-23029):

| problem | base | CT |
|---|---|---|
| has_majority | 29 | **22** |
| count_unique | 25 | 28 |

- Prediction 1 **failed**: has_majority -7, against an allowed -3.
- Prediction 2 **held**: count_unique +3, the required +3.
- **Decision: `--nested-canonical-tests` stays opt-in.**

Section 73's -2 on has_majority grew at 30 seeds instead of
disappearing, so it is a real cost. That contradicts the mechanism I
expected (has_majority's `t > n/2` gets *more* draws under CT), so the
cause is not known. A plausible guess, not tested: the old joint draw
also produced other tests that are correct for majority on these
instances. One example is `t >= n/2` combined with ExistsIf and
the count including the element itself. The canonical set removed
those, together with the true redundancies. Collapsing "equivalent"
draws on the domain t >= 0 is not neutral when several distinct tests
solve the same problem.

**Where count_unique stands.** It was 0 before section 71. Under the
current default (nested on, no CT) it is 6/10, 8/10 and 25/30 in the
three measurements: solved by the generic piece in most seeds, with no
skeleton.

## 75. A frequency-table piece -- design registered before implementing

Section 74 left count_unique at about 25/30 with the generic nested
piece, and showed that *replacing* draws can cost other problems. This
section *adds* one stateful piece. It is the classic "count per element"
table, not a count_unique shape.

**`--combinator-frequency`, opt-in, byte-identical when off.** It adds a
fifth stateful piece, `FrequencyTable`, drawn uniformly with the other
four when three loop slots are free. It runs on the whole-array or
length-prefixed layout:
1. **Table:** `aux[i]` = the number of j with cmp(elem(j), elem(i)),
   with cmp one of `==`, `<`, `>`. That gives the frequency table, or
   the rank of each element counted from below or from above.
2. **Pass over the table:** one existing single-pass relation over
   `aux[i]`. The relation is one of four:
   - the largest aux;
   - the smallest aux;
   - count-if `aux[i] cmp2 thr`;
   - exists `aux[i] cmp2 thr`.

   cmp2 is any of the six comparators, and thr is one of 0, 1, 2 or n/2.

It removes nothing, so every draw that used to solve has_majority is
still there.

**Protocol (as in section 71, with section 72's rule).**
1. Implement and freeze.
2. Write post-freeze problems and register predictions.
3. A/B over every registered array/scalar problem: base (current
   default) vs + `--combinator-frequency`, 10 matched seeds,
   `--step-limit 20000`, per-seed `--benchmark`.

Phase-1 sampling is reported as a diagnostic only. Time is not a gate.

### 75, post-freeze problems and predictions (registered after freeze 1ba125d, before running)

Four problems were written after the freeze, each with labels and a
verified reference program.
- **Predicted reachable:**
  - `elements_twice`: elements whose value occurs exactly twice
    (table `==`, count-if aux == 2);
  - `min_frequency`: occurrences of the least frequent value (table
    `==`, min aux).
- **Predicted unreachable** (the boundaries of the frozen design):
  - `mode`: the most frequent *value*. The pass returns aux, never the
    element at the argmax.
  - `values_twice`: *distinct* values occurring twice. That needs the
    count-if halved, and the pass has no division.

**A/B.** All 58 registered array/scalar problems, base (current default)
vs FR = + `--combinator-frequency`. 10 matched seeds (24000-24009),
section 71's budgets, `--step-limit 20000`, per-seed `--benchmark`.
Script: `scripts/run_frequency_ab.sh`.

**Predictions.**
1. elements_twice and min_frequency: FR >= base + 5 each, and > 0.
2. mode and values_twice: FR = 0.
3. count_unique: FR >= base.
4. No problem drops by more than 1.

**Decision rule, fixed now.** If 1, 3 and 4 hold, `--combinator-frequency`
becomes the default. Prediction 2 is a boundary check: if it fails, that
is reported as the piece being more general than designed, and it does
not block the promotion.

### 75 (results). Every success prediction holds; second_smallest -2 trips the no-regression rule

`data/frequency_ab_20261007/` (not versioned; 20:38-21:57; 1160 runs,
112 wall timeouts, on problems neither arm solves). Verified successes of
10 matched seeds (24000-24009), base / FR. Rows not listed are equal in
both arms.

| problem | base | FR |
|---|---|---|
| elements_twice (post-freeze) | 0 | **10** |
| min_frequency (post-freeze) | 0 | **10** |
| count_unique | 6 | **10** |
| count_leaders | 8 | 10 |
| min_gap | 6 | 7 |
| mode / values_twice (boundaries) | 0 / 0 | 0 / 0 |
| has_majority, max_frequency | 10 / 10 | 10 / 10 |
| max_gap_sorted | 7 | 6 |
| second_smallest | 10 | **8** |

- Prediction 1 **held** (+10 and +10).
- Prediction 2 **held**: both boundaries stay at 0.
- Prediction 3 **held**: count_unique +4, to 10/10.
- Prediction 4 **failed**: second_smallest dropped by 2.
- **Decision: by the fixed rule, `--combinator-frequency` stays opt-in.**

second_smallest is the same problem that tripped this rule in section 56
(8 -> 6), and it moved by 1-3 in sections 58, 61 and 71. It has never
held a 10-seed difference at 30 seeds. A 30-seed retest (as in sections
54, 58 and 74) would settle whether this is the same noise.

## 76. Frequency piece: 30-seed retest of second_smallest -- prediction registered before running

base vs FR (`--combinator-frequency`), second_smallest only, 30 fresh
matched seeds (25000-25029), section 75's budget, per-seed `--benchmark`.
Script: `scripts/run_frequency_retest_ab.sh`.

**Prediction:** FR >= base - 3 (of 30).

**Decision rule, fixed now.** If it holds, `--combinator-frequency`
becomes the default (section 75's other predictions already held).
Otherwise it stays opt-in.

## 76 (results). Noise, as before: second_smallest 24 = 24 of 30; `--combinator-frequency` becomes the default

`data/frequency_retest_20261007/` (not versioned; 22:15-22:17; 60 runs):
second_smallest base **24/30**, FR **24/30**. **The prediction held.** By
the rule, `--combinator-frequency` is now the CLI default, with no
exception needed. `--no-combinator-frequency` restores the old behavior.
GenConfig's own default stays false, so the byte-identity tests are
unchanged.

second_smallest has now failed a 10-seed no-regression check three
times (sections 56, 75 and, as a near miss, 58) and has never held one at
30 seeds. Future batteries should either run it at 30 seeds from the
start or treat its 10-seed differences of 2 as uninformative.

## 77. A pointer-following piece -- design registered before implementing

cycle is the last of section 70's five. It needs a loop whose next index
comes from the data. Per the standing rule (generic pieces before
skeletons), this is a new combinator family, not a cycle skeleton. This
also retires cycle as the held-out "ruler" it has been since sections 25
and 38. The boundaries below take over that role.

**`--combinator-follow`, opt-in, byte-identical when off.** With the flag
on, one combinator block in eight is a follow block.

- **Layout.** Node i's pointer is `arr[base + i]`.
  - `base = 1` with `start = arr[0]`: head-prefixed, cycle's encoding.
  - `base = 0` with `start = 0`.
  - `nodes = v0 - base`. A pointer outside `[0, nodes)` ends the walk,
    which acts as the null.
- **Loop.** `p = start; steps = 0; while (steps < v0 && 0 <= p < nodes)
  { relation; p = arr[base + p]; steps++ }`. The cap of v0 steps is the
  pigeonhole bound.
- **Relation**, one of:
  - `CountSteps`: acc = steps;
  - `StillInRange`: acc = 1 if p is still in range after the loop, i.e.
    the walk never ended;
  - `VisitsKey`: acc = 1 if p == k at some step, k one of 0..2;
  - `TwoSpeed`: a second pointer q advances two steps per iteration,
    each step range-checked; acc = 1 if p == q after a move (Floyd).

**Protocol** (sections 71-76, with section 72's rule):
1. Implement and freeze.
2. Write post-freeze problems on cycle's encoding and register
   predictions.
3. A/B: cycle and the post-freeze problems at 30 seeds, plus all
   registered array/scalar problems at 10 seeds for no-regression.

Time is not a gate.

### 77, post-freeze problems and predictions (registered after freeze 247b473, before running)

`ListQueryProblem` relabels cycle's own instances (same lists, other
questions). Four problems were written after the freeze, each with labels
and a verified reference program (`tests/list_query_tests.cpp`).
- **Predicted reachable:**
  - `walk_length`: nodes before null, or n + 1 if the walk never ends
    (CountSteps);
  - `visits_zero`: whether the walk visits node 0 (VisitsKey).
- **Predicted unreachable** (boundaries of the frozen design, the new
  rulers):
  - `cycle_length`: the length of the cycle entered. That needs a
    second walk around the cycle.
  - `last_node`: the node before null. No relation keeps the previous
    node.

**Batteries.** base (current default) vs FL = + `--combinator-follow`,
per-seed `--benchmark`, `--step-limit 20000`.
- (i) cycle and the four list problems: 30 matched seeds (26000-26029),
  max-n 5, 180 s wall (cycle's budget from section 56).
- (ii) every registered array/scalar problem: 10 matched seeds
  (27000-27009), for no-regression.

**Predictions.**
1. cycle: FL >= base + 15 (of 30).
2. walk_length and visits_zero: FL >= base + 15 each.
3. cycle_length and last_node: FL = 0.
4. No problem in (ii) drops by more than 1. Per section 76,
   second_smallest's 10-seed drops of 2 are retested at 30 seeds before
   counting.

**Decision rule, fixed now.** If 1, 2 and 4 hold, `--combinator-follow`
becomes the default. Prediction 3 is a boundary check and does not block.

### 77 (results). cycle 0 -> 30/30, both reachable list problems solved, boundaries hold; max_gap_sorted -2 trips no-regression

**Battery (i)** (`data/follow_lists_ab_20261008/`, not versioned;
22:23-22:40). Verified successes of 30 matched seeds, base / FL;
"rej" counts seeds where a SUCCESS-labeled program was rejected by
re-verification.

| problem | base | FL |
|---|---|---|
| cycle | 0 (rej 21) | **30** |
| walk_length (post-freeze) | 0 | **30** |
| visits_zero (post-freeze) | 0 (rej 28) | **22** (rej 7) |
| cycle_length (boundary) | 0 | 0 |
| last_node (boundary) | 0 | 0 |

**Battery (ii)** (`data/follow_ab_20261008/`, not versioned;
22:40-23:41). 10 matched seeds; rows not listed are equal in both arms:
cycle 0 -> 10, min_gap 8 -> 9, second_smallest 10 -> 9,
**max_gap_sorted 10 -> 8**.

- Prediction 1 **held** (cycle +30).
- Prediction 2 **held** (+30, +22).
- Prediction 3 **held**: both new rulers stay at 0.
- Prediction 4 **failed**: max_gap_sorted dropped by 2.
- **Decision: `--combinator-follow` stays opt-in pending a 30-seed
  retest of max_gap_sorted.**

That retest is the same treatment section 76 gave second_smallest.
max_gap_sorted also moved by 1-2 in sections 56 and 75 without a cause.
The base column of battery (i) shows what the piece fixed: 21 of 30 cycle
seeds and 28 of 30 visits_zero seeds produced SUCCESS-labeled programs
that were rejected on re-verification. Those were fits, which is exactly
section 25's warning about this problem. With the piece, cycle has no
rejections at all.

## 78. Follow piece: 30-seed retest of max_gap_sorted -- prediction registered before running

base vs FL (`--combinator-follow`), max_gap_sorted only, 30 fresh
matched seeds (28000-28029), section 77's budget (max-n 8, 180 s),
per-seed `--benchmark`. Script: `scripts/run_follow_retest_ab.sh`.

**Prediction:** FL >= base - 3 (of 30).

**Decision rule, fixed now.** If it holds, `--combinator-follow` becomes
the default (section 77's other predictions already held). Otherwise it
stays opt-in.
