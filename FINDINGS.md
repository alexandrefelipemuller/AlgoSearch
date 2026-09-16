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
