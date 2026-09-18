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
