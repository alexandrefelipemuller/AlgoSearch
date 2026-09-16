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

## 10. Raw compute, no skeleton, can't buy past the wall either

Direct follow-up to the user's objection to `loopAccumulatorBias`: doesn't
biasing the generator toward a canonical loop *shape* risk boxing the
search into only ever finding algorithms that fit that shape, at the cost
of genuinely novel control-flow structures? Real concern -- so instead of
arguing it, tested it: could raw compute substitute for the skeleton bias
on the three problems with a documented "wall" (`gcd`, `is_prime`,
`factorial`)? `loop-bias=0.0` (no skeleton at all), and *much* more of
everything else than any prior run on these problems at this bias level:
beam=1500 (vs 200-500), candidates=5000 (vs 800-2000), max-program-size=400
(vs 60-150), 10 seeds, 900s wall cap each, `ALGOSEARCH_DEBUG_LANDSCAPE=1`
for a per-round diagnostic (script:
`scripts/run_pure_bruteforce_20260916.sh`, output in
`data/pure_bruteforce_deep_20260916_1555/`).

**Result: zero solutions, all three problems, every seed that finished.**
- `gcd`: stuck at 63/100 on LEVEL 1 for the entire 900s in every completed
  seed (3/10 seeds finished in the window -- each seed alone now costs
  ~5 minutes at this budget). `best size` stayed 4 the whole run even as
  the size cap climbed to 400+ -- growing the program past the tiny
  63-scoring one never helps.
- `is_prime`: also never left LEVEL 1; best score ranged 7-9/11 across
  seeds (a little better than the modest-budget runs' flat 7/11, so more
  compute *did* move the needle slightly here) but never reached 11/11.
- `factorial`: didn't even clear its trivial LEVEL 1 (6 instances,
  best 3-4/6) -- confirms finding #5's min/max/sum-without-skeleton result
  generalizes to factorial too: the loop-accumulator skeleton isn't
  optional scaffolding for these problems, it's load-bearing for
  composing *any* working accumulation loop at all, independent of gcd's
  separate scalar-magnitude wall.

The landscape diagnostic makes the shape of the wall concrete, not just
its score: at every one of ~200+ program sizes sampled for `gcd`, roughly
1600 of ~1650 scored candidates per round are the *exact same* 63-point
local optimum (`topScores` dominated by one value, only 2-5 distinct
behaviors near the ceiling) -- the beam isn't failing to explore, it has
already converged completely and additional size/candidates/seeds just
keep re-finding the identical plateau. This is the same "genuine
discontinuity" diagnosis from the behavior-niche/annealing/score-band
experiments (all six prior selection/diversity mechanisms failed
identically here) -- confirmed again, this time by scaling raw search
budget by ~5-10x instead of changing the selection mechanism, with the
same null result. Compute is not the missing ingredient for these three
problems at `loop-bias=0.0`; something about the local neighborhood
around a full solution is structurally unreachable by single mutations
from this 63/100 (or equivalent) plateau, and no amount of beam width or
candidate count changes that geometry.

Honest tension this leaves unresolved: the skeleton bias *does* fix
gcd/mod/factorial in practice (section 9's sweep solved gcd cleanly, and
section 5/6 already showed min/max/sum need the skeleton too) -- but this
result confirms the user's underlying worry has teeth: the skeleton isn't
a convenience that a bigger budget could replace, it's currently the
*only* thing that gets these problems off their local optimum at all. A
search that can only escape this specific wall via one specific
hand-designed control-flow prior hasn't demonstrated it can find
algorithms whose correct shape doesn't happen to be "a counting loop" --
that's a real, currently-open gap between "this project finds algorithms"
and "this project discovers novel algorithm structure," not yet closed by
anything tried so far.

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

## 11. Islands (FunSearch-style parallel populations): implemented, tested, no win on this wall

Direct response to a fair objection about `loopAccumulatorBias`: doesn't a
fixed shape-prior risk boxing the search into only ever finding algorithms
that fit that shape? Rather than argue it, implemented and tested the
alternative the user proposed: multiple independent populations
("islands"), each under a different `GenConfig` (varying
`loopAccumulatorBias`), evolving in parallel with periodic migration of
the current best-scoring program into every other island (replacing its
weakest beam members). New, strictly additive code path --
`SearchConfig::islandGenConfigs` / `BeamSearch::runIslands()` in
`src/search/BeamSearch.{hpp,cpp}`, wired to `--island-loop-biases`,
`--migration-interval`, `--migration-rate` in `main.cpp`. `islandGenConfigs`
empty/size<=1 (the default) reproduces the original single-population
`run()` path byte-for-byte -- verified via `make test` 49/49 after the
change, no regression.

**First test was confounded.** Ran 4 islands (biases 0.4/0.2/0.0/0.6) on
gcd/is_prime/factorial -- all three "solved" in seconds, looked like a
clean win. It wasn't: island 0 (bias=0.4, ALREADY known from section 9 to
solve gcd/factorial cleanly alone) won every single time
(`winning island 0` in every log), migration or not. Including a
proven-good strategy in the mix and then declaring victory when it wins is
not a test of anything -- caught this before writing it up.

**Redesigned properly** (`scripts/run_islands_experiment.sh`, data in
`data/islands_experiment2_20260916_1657/`): mix excludes the known-good
0.4 entirely (`0.0, 0.15, 0.25, 0.35`), gcd + is_prime only (factorial's
"wall" was already explained away in section 10), matched-budget solo
control run at each of the 4 individual bias values, 3 seeds, `--benchmark`
on every run. Result: **islands added no measurable benefit**, but the
reason is itself the interesting finding --
- `loop-bias=0.0` alone: genuinely stuck on both problems (gcd timed out
  at the 300s cap with zero output; is_prime found 0 solutions in the
  window) -- consistent with sections 9/10's "wall."
- **Every other bias in the mix -- 0.15, 0.25, or 0.35, each alone --
  already solves both problems cleanly and fast** (0.03-0.10s search time,
  0 candidates rejected by the adversarial recheck). The "wall" turns out
  to be specifically a zero-vs-nonzero-skeleton-bias cliff, not a
  fine-tuned "needs exactly 0.4" requirement, and not a landscape with
  multiple genuinely different solution neighborhoods that would need
  different strategies to reach.
- Islands did solve both (0 rejected), but at 3-4x the wall-clock cost of
  any single solo run in the mix (0.11-0.29s vs 0.03-0.10s) -- paying for
  4 populations' worth of generation/scoring per round when only one
  non-zero-bias island was ever going to be needed.

Honest conclusion: islands are implemented and work correctly (migration,
promotion, and the invariance/persistence pipeline all verified against
the existing single-population logic), but this particular wall isn't the
kind of problem islands are suited to solve -- it doesn't need strategy
diversity, it needs *any* nonzero skeleton probability at all, which a
single population already provides. This doesn't mean islands are useless
generally: they're a better fit for a landscape that's genuinely
multi-modal in a way no single fixed bias handles well across the board
(sections 8/9 haven't found a single bias that cleanly solves sorting
past N=5, or that reproduces mod's 100/100 seed reliably rather than 1/8)
-- **an open follow-up is retrying islands specifically on `mod` or
`sorting`, not on a wall that turned out to be a simple on/off switch.**

**Follow-up: verifying migration itself actually works, not just the
experiment's outcome.** Tried to catch it live: ran min/max/sum/contains/
gcd/is_prime with 3 crippled (bias=0.0) islands + 1 working (bias=0.4)
island, `migrationInterval=1, migrationRate=1.0`, plus a new
`ALGOSEARCH_DEBUG_ISLANDS` env var (mirrors `ALGOSEARCH_DEBUG_LANDSCAPE`)
printing every island's own best score each round. Result: **every
problem tested solves within 1-2 program sizes once any working-bias
island is present** -- even deliberately starving all four islands
(beam=20, candidates=60) didn't slow gcd down. That's informative on its
own (confirms finding #9's skeleton-bias power is not resource-dependent)
but it means there's no multi-round window in this problem suite where a
crippled island visibly lags for a while and then visibly catches up mid-
search -- the "good" island (or, at trivial N=1 levels, EVERY island by
sheer luck) finds a full solve too fast to observe a live transplant.

So migration's correctness is verified two other ways instead, both now
permanent: (1) a controlled unit test
(`islands_with_a_crippled_majority_still_solve_what_the_crippled_config_alone_cannot`,
`tests/beam_search_tests.cpp`) that first confirms loop-bias=0.0 alone
never reaches N=2 on `min` at a tiny budget (`islands_disabled_single_
population_with_zero_bias_finds_nothing_at_this_budget`), then confirms
the SAME crippled config as 3 of 4 islands, plus one working island,
does reach N=2 with a confirmed full solve -- and (2)
`a_single_island_config_falls_back_to_the_original_single_population_path`,
confirming the size-based dispatch (`islandGenConfigs.size() > 1` routes
to `runIslands()`, everything else uses the original path unchanged)
behaves exactly as documented. `make test`: 52/52 (49 original + these
3), build clean under `-Wall -Wextra`.
