# AlgoSearch

AlgoSearch is a C++20 program-synthesis framework that discovers algorithms
by beam search over a small imperative AST language ("Mini-C"), with **no
machine learning involved** — no neural network, no gradient descent, no
learned model of any kind. Candidate programs are generated from a
probabilistic grammar, executed against test instances by a tree-walking
interpreter, scored, and the best-scoring candidates survive to the next
round while the program is grown one node at a time.

The interesting part isn't "beam search finds programs" — it's the
experimental record of *why* it does or doesn't, kept in
[`FINDINGS.md`](FINDINGS.md). Read that file before touching `src/search/`
or `src/language/` — it documents which levers (population diversity,
grammar bias, native operators) actually mattered and which didn't, with
pointers to the raw data backing each claim.

## How it works

1. **Mini-C**: a minimal language with scalar variables, one array, `if`,
   `while`, `swap`, native comparisons (`<`, `>`, `==`, `<=`, `>=`, `!=`),
   arithmetic (`+`, `-`, `*`, `/`, `%`), and bitwise operators (`&`, `|`,
   `^`, `<<`, `>>`) — the arithmetic/bitwise extension is recent (see
   `FINDINGS.md` section 17) and gated to an 8% substitution probability
   so it doesn't dilute the tuned grammar every earlier problem depends on.
2. **Generator**: builds random Mini-C programs from a grammar, optionally
   biased toward a canonical counting-loop idiom
   (`GenConfig::loopAccumulatorBias`) — see `FINDINGS.md` for why that
   bias exists and what it does and doesn't buy you.
3. **Interpreter**: executes a candidate against an instance (an input
   array) and returns the resulting array, with a step-count limit so a
   runaway/infinite loop just times out instead of hanging the search.
4. **Beam search**: scores every candidate against every training
   instance at the current difficulty level, keeps the best `beamWidth`
   programs, grows them, repeats. A level is only labeled a confirmed
   `SUCCESS` if the same unmutated program also holds several levels
   further ahead (`invarianceLookahead`) — see `FINDINGS.md` section 4.
5. **Benchmark / independent verification** (`--benchmark`): never trusts
   the search's own `SUCCESS` label at face value. Every candidate is
   re-run against held-out sizes, structurally adversarial instances
   (duplicates, negatives, sorted/reversed/all-equal), and a magnitude
   shift far outside the training range. Only candidates that survive
   this get reported, with an empirically fit time-complexity curve.

## Building and testing

```sh
make            # builds ./algosearch
make test       # builds and runs ./algosearch_tests (58 unit/integration tests)
```

## Running a search

```sh
./algosearch --problem gcd --max-n 20 --beam-width 500 --candidates 1500 \
             --max-program-size 150 --seed 1000 --benchmark --benchmark-seeds 5
```

Run `./algosearch --help` for the full flag list (search width/depth,
diversity/crossover/niching knobs, grammar-bias knobs, etc. — all of them
are either "confirmed default" or "opt-in, mixed evidence" per
`FINDINGS.md`).

## Results: all 17 problems, one modest-budget sweep

The table below is from a single sweep (2026-10-03, `data/modest_battery_combinator_default_20261003/`)
across every problem this project currently supports, at the **same modest
search budget** for each (`--beam-width 500 --candidates 1500
--max-program-size 150 --benchmark-seeds 5`, current grammar defaults --
`loopAccumulatorBias=0.4`, native `Le`/`Ge`/`Ne`, native `Mul`/`Div`/`Mod`/
bitwise, the skeleton library from `FINDINGS.md` sections 11-17 (minus the
`counting` and `divideconquer` skeletons, removed in section 57 after an
isolation battery showed nothing needs them), and the combinator algebra on
by default, `--combinator-bias 0.5`, since section 48). The table predates
the stateful combinator pieces (Kadane, LIS, rain water and relatives),
on by default since section 62. With them, ten more array problems are
solved 10/10 (section 61); `--no-combinator-state` reproduces the table. The nested aggregates and
absolute step (default since section 72) add six more: max_adjacent_gap,
max_profit, count_leaders, has_majority, count_dominated and count_unique
(section 71); `--no-combinator-nested` turns them off. The frequency-table piece
(default since section 76) adds elements_twice and min_frequency and takes
count_unique to 10/10 (section 75); `--no-combinator-frequency` turns it off. Pointer-following blocks,
drawn only when every input value is a valid index (default since section 79),
solve cycle, walk_length and visits_zero (sections 77-79);
`--no-follow-pointer-inputs` turns them off. Orthogonal modifiers (difference
direction and |.| on every step relation, default since section 81) solve
max_drawdown, total_variation and largest_drop_step (section 80);
`--no-combinator-orthogonal` turns them off. Arg-best (return where the best
is, default since section 85) solves mode, least_frequent_value and
argmax_index (section 82); `--no-combinator-argbest` turns it off. Distinct counting (default
since section 88) solves values_twice, repeated_values, rare_values and
frequent_values (section 87); `--no-combinator-distinct` turns it off. Every level's scoring set
also holds the first 12 instances of each earlier level (`--regression-guard
12`, default since section 89), so a champion can no longer break smaller
n unnoticed; `--regression-guard 0` turns it off.
Every row is a solution that survived `--benchmark`'s independent adversarial
re-check, not just the training score. "Time to generate" is the search
wall-clock time per seed (mean across the 5 benchmark seeds) on an 8-core
machine -- how long it took to *find* the solution, not how fast the
discovered algorithm runs. The previous sweep (2026-09-17, combinator off,
`data/modest_battery_20260917_1518/`) is reproducible with
`--combinator-bias 0`; the main changes are `reverse` (O(n²), ~16s -> O(n),
~0.2s) and `graph_reachability` (5 rejections -> 0).

| Problem | Result | Complexity | Time to generate (avg/seed) |
|---|---|---|---|
| `sorting` | 5 solutions confirmed | O(n²) | ~0.06s |
| `min` | 4 solutions confirmed | O(n) | ~0.02s |
| `max` | 5 solutions confirmed | O(n) | ~0.02s |
| `sum` | 5 solutions confirmed | O(n) | ~0.02s |
| `contains` | 5 solutions confirmed | O(n) | ~0.02s |
| `contains_ordered` | 5 solutions confirmed | O(n) | ~0.03s |
| `is_prime` | 4 solutions confirmed | **O(√n)** | ~0.02s |
| `gcd` | 4 solutions confirmed (native-mod Euclidean) | **O(1)** | ~0.04s |
| `factorial` | 5 solutions confirmed | O(n²) | ~25.7s |
| `fibonacci` | 5 solutions confirmed | O(n) | ~0.02s |
| `count` | 5 solutions confirmed | O(n) | ~0.06s |
| `average` | 5 solutions confirmed | O(n) | ~0.02s |
| `reverse` | 5 solutions confirmed (two converging pointers) | **O(n)** | ~0.17s |
| `power` | 5 solutions confirmed | O(n) | ~0.02s |
| `three_sum` | 5 solutions confirmed | O(n²)\* | ~0.24s |
| `single_number` | 5 solutions confirmed (XOR reduce) | O(n) | ~0.02s |
| `graph_reachability` | 3 solutions confirmed, 0 rejected | O(n^1.5) | ~0.18s |

\* the algorithm is a genuine triple-nested O(n³) loop; the curve fit's
exponent (~2.1, r²=0.91-0.92) is measured over a small `n` range (max-n=8)
where constant-factor effects still dominate the fit -- see the O(n log n)
heapsort caveat below for the same "not a pure power law over a bounded
range" nuance.

Results worth calling out:

- **`gcd` now reaches genuine O(1), and `is_prime` genuine O(√n) --
  the "language ceiling" from `FINDINGS.md` section 13 is broken.**
  Section 13 found that a smarter loop bound (mod-based GCD, sqrt-bound
  primality) never paid off, because each candidate's own check was
  still built from repeated subtraction (O(a/b) or O(x/d) per check),
  so total work stayed linear regardless of how few candidates the outer
  loop considered. `FINDINGS.md` section 17 added native `Mul`/`Div`/`Mod`
  to Mini-C, and the free search converges onto the native-mod variant of
  both problems in every seed tested, confirmed by hand-timed A/B
  (`gcd(n,1)`: exponent 0.99 -> ~0.00 across a 100x range; `is_prime` on
  large primes: 10,544x faster at the larger test point) and now by
  `--benchmark`'s own curve fit, shown in the table above.
  `gcd`'s solution:
  ```
  v1 = arr[0]; v2 = arr[1];
  while (v2 != 0) {
    v3 = v1 % v2;
    v1 = v2;
    v2 = v3;
  }
  arr[0] = v1;
  ```
- **`sorting` CAN reach O(n log n)** — a from-scratch heapsort skeleton
  (`FINDINGS.md` section 14) measures a genuinely different complexity
  signature (exponent 1.13-1.26) from the selection-sort/bubble shape
  the free search always converges on instead (exponent 1.90-2.00, what
  shows in the table above). The free search never picks the bigger
  heapsort AST over the smaller quadratic one on its own — reaching it
  currently needs the research-only `ALGOSEARCH_FORCE_SKELETON=heapsort`
  override, not the default `--benchmark` run.
- **`three_sum`, `single_number`, `graph_reachability` are new**
  (`FINDINGS.md` sections 15-17), added to test coverage against
  algorithm families the original problem set didn't touch at all:
  relating three array elements, bit manipulation, and graphs. Two very
  different stories: `single_number` (LeetCode's classic XOR trick) was
  nearly free — one new accumulator variant reusing existing
  infrastructure, solved by the free search in ~0.03s. `three_sum` and
  `graph_reachability` needed dedicated skeletons the free grammar had no
  chance of stumbling into on its own (3SUM: confirmed by a 65-minute,
  10x-budget run finding zero genuinely generalizing solutions before the
  skeleton was added; graph reachability needed the native-multiply
  extension just to make the adjacency-matrix indexing *representable* at
  all, plus a real bug fix in `Benchmark.cpp`'s magnitude-shift check,
  which was corrupting the vertex-count/edge-flag metadata it was
  shifting). In the 2026-09-17 sweep `graph_reachability` also had a lower
  confirmation rate (1 of 6 SUCCESS candidates survived independent
  re-verification); in the current sweep 3 of 3 survive. See section 17
  for the standalone correctness verification (8/8 hand-built test graphs)
  done before ever trusting the search on it.
- **The combinator algebra is on by default** (`FINDINGS.md` sections
  38-48): blocks drawn as a product of input layout x loop shape x
  relation, instead of whole hand-written algorithms. Against the previous
  default (section 47, 15 matched seeds per problem) it loses nothing on
  the 17 problems above, and takes eight newer array problems
  (`substring`, `palindrome`, `inversion_count`, `has_duplicate`,
  `is_sorted`, `count_descents`, `strictly_increasing`,
  `has_adjacent_equal`) from 0-4/15 to 15/15. With every sort skeleton
  disabled it still sorts, by algorithms no skeleton contains: Fung's "I
  can't believe it can sort", exchange sort, and occasionally gnome and
  bubble sort (section 46).
- **`factorial`**'s remaining "failures" at its hardest training level
  aren't a search failure: `factorial(27)` overflows a 32-bit `int`, so
  the discovered algorithm is actually correct, just bounded by the
  interpreter's integer type.
- **`count`/`average`/`power`** (added to isolate compositional
  primitives the original ten didn't exercise: a counter gated by a
  condition, a division-via-subtraction skeleton, and a
  constant-multiplicand nested multiply as opposed to factorial's
  increasing-multiplicand one) all plateaued at first, diagnosed the
  same way as `gcd`'s original `Sub(Var,Var)`/`!=` gaps and fixed the
  same way (one dedicated skeleton per shape) — see `FINDINGS.md`
  section 10.

See `FINDINGS.md` for the full experimental history (grammar bias sweeps,
diversity/crossover/niching A/B tests, the native-comparison-operator
change, the cross-skeleton study, the complexity-ceiling investigation and
how it was later broken, heapsort, 3SUM, and the honest negative results
along the way) and `scripts/run_full_battery_modest.sh` to reproduce this
sweep.

## Held-out discovery benchmark (in progress)

Three problems deliberately **not** in the skeleton library now exist as the
ruler for open-ended discovery work: `lis`, `substring`, and `cycle`
(`src/problems/LisProblem.{hpp,cpp}`, `SubstringProblem.{hpp,cpp}`,
`CycleDetectionProblem.{hpp,cpp}`). The baseline
(`scripts/run_heldout_baseline.sh`, `data/heldout_baseline_20260920/`) froze
the starting line: 0/30 genuinely generalizing solutions across both arms
(default `loop-bias=0.4` and the `loop-bias=0.0` isolation setting) at a
matched modest budget. Etapa 2 added the opt-in divergence engine
(`--post-solve-sizes`, default 0) and ran it against the same ruler
(`scripts/run_heldout_divergence_ab.sh`): still 0/30 generalizing solutions
-- the window archives ~20-29 alternative gambiarras per run but creates no
new shape, confirming that the bottleneck is construction, not survival.
Etapa 3 added the opt-in composite-primitives operator (`--composite-bias`,
default 0.0): a two-loop family assembled from section 24's catalogued
primitives under one shared binding context. On the same ruler it COMPOSES
the target `substring` shape (champions contain the correct `n-m+1` bound
and alignment indices) but never converges to the clean version -- 0/15
generalizing in Test A, with `lis` (Test B) and `cycle` (reserve) out of
reach by construction, as designed.
See `FINDINGS.md` sections 25-27 for the protocol, the five-column taxonomy,
and the next steps.

Generation-time pruning (`--prune-generation`, opt-in, off by default and
byte-identical to the unpruned grammar when off) redraws text-level trivia at
the moment it is generated: `v = v`, `arr[e] = arr[e]`, `swap(e, e)`, an
if/while condition that is constant by its own text, and a literal operand
that always errors (`/ 0`). Set `ALGOSEARCH_DEBUG_PRUNE_COUNTS=1` to print how
often each shape is drawn. A matched-seed A/B found it neutral: it removes
5-8% junk per site without changing success rate or wall time -- see
`FINDINGS.md` section 33.

The combinator algebra (`--combinator-bias`, opt-in) builds a block as a
product of layout x loop x relation axes from catalogued primitives under one
shared binding context. With no named skeleton available it solves sum, min,
max, count, contains, single_number, reverse, sorting (as Fung's "I can't
believe it can sort") and substring, plus three problems written after its
axes were frozen (`palindrome`, `inversion_count`, `has_duplicate`); it
cannot reach `is_sorted`, `cycle` or `lis`, as predicted. `--benchmark` now
also checks every small input for problems that define
`Problem::exhaustiveInputs`. See `FINDINGS.md` sections 38-39.
