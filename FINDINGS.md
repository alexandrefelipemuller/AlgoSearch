# Findings summary

The full log (222 numbered section headers, 526 KB) is in `docs/findings/`, split into 11
chunk files in the order it was written. Nothing was deleted: the chunks
concatenate to the old `FINDINGS.md` byte for byte (git tag `pre-summary`).
**Read this file first; open a chunk only for the section it points to.**

    grep -n '^| 104' docs/findings/INDEX.md      # which chunk and line holds section 104
    sed -n '1,80p' docs/findings/09_sections_100-111.md

Section numbers below refer to that log. Code comments that say "FINDINGS.md
section N" mean the same numbers. When you add a section, append it to the
last chunk and add one line here (state, decision table or lessons).

## 1. What the tool is

C++20 program synthesis: beam search over a small imperative language
("Mini-C": int variables, `arr`, an `aux` array, `swap`, `while`, `if`,
`call(lo, hi)` recursion), no machine learning. A problem supplies instances
and `isSolution`. A program is promoted level by level (N = 1, 2, ...); a
final check re-verifies it outside the search (`--benchmark`, adversarial and
exhaustive inputs). Selection is by exact solved count; a candidate pool of
mutated, fresh and skeleton-seeded programs feeds the beam.

## 2. Current defaults and how they got there

Generic pieces that are on by default (each was promoted by a matched-seed A/B
and, from section 99 on, a joint battery with a no-drop-of-2 rule):

| default | section | note |
|---|---|---|
| combinator algebra (product of catalogued axes) | 38, 47, 48 | the core generic mechanism |
| adjacent-pair axis | 40, 42 | |
| step axes (`--combinator-step`) | 52, 53 | |
| `--tail-enum 10` (enumerate the small tail after a block) | 51, 54 | |
| stateful pieces (Kadane, rain water, LIS) | 55, 61, 62 | |
| nested aggregates | 71, 72 | |
| frequency table | 75, 76 | |
| pointer-following piece, gated on pointer-like inputs | 77-79, 90 | ungated version cost max_gap_sorted (78) |
| orthogonal modifiers (direction, abs) | 80, 81 | |
| arg-best | 82-85 | |
| distinct counting + regression guard 12 | 86-89 | the guard removed the n = 1 rejections |
| `--confirm-reseed 12` | 97 | fresh-seed confirmation against false positives |
| scalar-loop piece (+ index range) | 100, 101 | replaced four scalar skeletons and `average` |
| triple aggregate | 103 | replaced `threesum` |
| matrix relaxation | 104, 105 | replaced `graphreachability`; `shortestpath` never worked |
| grid DP | 109 | replaced `dtw` |
| per-term transform | 111 | |
| distinct aggregate (max on DistinctCount) | 112b-112d | the first version cost max_repeated_value (a deceptive attractor) |
| arg sentinel guards | 113 | |
| `--local-cond-mutation 0.3` | 115, 115b | 44 vs 21 of 150 |

Hand-written skeletons still in the library (all shown indispensable or
undecidable in section 99/102/107/108): `selectionsort` (102b, closed),
`heapsort`, `binarysearch` and `mergesort` (inert in the default, 108),
`tspbruteforce`, `tspbranchandbound` (107b-107d). Removed along the way:
`counting`, `divideconquer` (57), four scalar skeletons (100), `average` (101),
`threesum` (103), `graphreachability` (104), `shortestpath` (105), `dtw` (109).
`ALGOSEARCH_DISABLE_SKELETON=name` turns one off for an ablation.

Reference numbers: joint battery 717 of 730 solved (113); external held-out
set from a public list 4 of 30 (114), i.e. the battery overstates how far the
system generalizes.

## 3. Opt-in, kept

| flag | section | why it is not default |
|---|---|---|
| `--profile-novelty W`, `--profile-ban` | 98, 118 | compresses the archive ~100x, finds heap sort in 8/8 seeds, but no new family; the ban adds <= 2 classes per seed |
| `--post-solve-sizes N`, `--restart-on-stall K`, `--wall-seconds S` | 92-96 | restarts multiply variants, not families |
| `--first-index`, `--prefix-split` | 116, 117, 117b | close two gaps; PrefixSplit costs frequent_mode_or_none |
| `--combinator-follow` (ungated), `--combinator-v1` | 77-79 | ablation |
| `--recursion-call-bias`, `--second-array-bias` | -- | recursion seeding (`v1/v2 = 0/n`) and the `aux` array; off unless a problem needs them |
| `--dc-shell-search` (+`-key`, `-end`) | 121-124 | assembles O(log n) search on three specifications; costs nothing on the 73-problem battery; no measured gain in solved counts, so not promoted |
| `--max-executions N` | 126 | execution cap in PSB2 units; `Psb2Problem` + `scripts/run_psb2.sh` run seven integer PSB2 problems under the suite's protocol |
| `--term-expr P` | 126b, 127, 129, 130 | enumerated `x op c1 op c2` term in loop sums; fuel_cost 0/30 -> 29/30 (paired_digits 4 -> 2); DEFAULT 0.3 since 130 |
| `--prefix-cross P` | 128, 128b, 129, 130 | first index where a running sum crosses a threshold; basement 0/30 -> 30/30; DEFAULT 0.3 since 130; joint battery with --term-expr: 755/770 both arms (129r) |
| `--pair-find P`, `--parity-term P` | 130 | pair-that-hits-a-target block; positional (parity) digit-fold sum; opt-in, find_pair and luhn solved in screens |
| `--forbid-stmt`, `--fill-bsearch-hole`, `--ablate-skeleton`, `--fill-tsp-guard` with `--soundness-labels --metamorphic --rank-worst-scale` | 63-69, 43-44, 118 | research modes; the TSP guard pipeline (43-44) is a positive result and the only code that reproduces it |

## 4. Closed as negative (do not retry without a new idea)

- Population diversity, niching, crossover, annealing, score bands as ways to
  find new algorithms (5, 20, 21, 28; later removed). The grammar prior is what
  matters (5).
- Shape-only skeletons, enumerative repair of scrambled skeletons, joint pairs
  and SMT sketching (66-68): shape alone does not compose; 100 of 112
  skeleton sites are rebuilt by enumeration but not the shape (65).
- Adaptive family weights and a protected skeleton quota (59, 60).
- Nested placement to replace `selectionsort` (102, 102b), canonical outer
  tests (73, 74).
- Role-typed variables (37), generation-time pruning (33-35).
- Two-call divide-and-conquer shell (119): never produces a recursive
  reduction.
- Third interval-search shape, minimum of a rotated array (123): 0 hits.
- Sorting without swap and without the merge skeleton (118b, 118c): 0
  generalizing programs. Selection and demotion levers are exhausted; the
  limit is reachability in the grammar (118).

## 5. Results worth knowing

- Sorting reaches heap sort in a minute and O(n log n) (14, 92); bubble and
  gnome sort appear by composition when the sort skeletons are off (46);
  there are two or three algorithm families only (92, 93).
- `max_subarray_sum`: only Kadane (96). `three_sum`: only the skeleton (49).
- Open-specification problems (`any_peak`, `any_valley`, `any_duplicate`,
  property checker instead of one reference answer, 120): `any_peak` 10/10 with
  six behavior classes, all linear scans.
- One-call interval shell (121-124): binary-search peak in 9 of 10 seeds, key
  membership search in 5 of 10 verified (121c). This is assembly of a
  known algorithm from a small piece that already carries the shape (key,
  clamp, flag), not a discovery of binary search. Not promoted (124).
- Non-canonical finds so far are small: gnome sort with an uninitialized
  variable, Fung's sort, a point mutation for `has_adjacent_equal`.
- Open question the log keeps returning to: how to get an algorithm no one
  wrote down. Candidates not yet tried: cost-optimal specifications (fewest
  comparisons) and neutral drift on large programs.

## 6. Lessons that change how experiments are run

- Register predictions and the decision rule before running; report negatives.
- Matched seeds. Joint battery before promoting; time is not a gate (promote on
  reachability and diversity).
- `--start-level 2` when level 1 is trivial (`arr[0] = 0`), otherwise every
  record is UNCONFIRMED (119 round 1, 120). The converse hole: a run that
  starts at level 2 never trains on a one-element input, so adversarial sets
  must cover every size (121c, 122).
- `--profile-ban` and `--post-solve-sizes` depress base counts (rotated_min
  3/10 with them, 10/10 without, 124): compare like with like.
- Python triage must seed `v1/v2 = 0/n` when `--recursion-call-bias > 0`
  (`scripts/triage_open_spec.py --seed-rec`); otherwise every recursive program
  looks wrong.
- Verify a "hit" outside the engine (400 inputs, sizes 1-256); the engine's
  adversarial set missed 3 of 8 wrong variants once (121c).
- Long runs: `setsid nohup`, never `pkill -f` on the command's own text.
- Reproducing an old section: the cleanup of sections 125-125c removed 27
  flags and moved 40 scripts to `scripts/archive/`; `git checkout pre-cleanup`
  (or `pre-cleanup2`, `pre-cleanup3`) runs them unchanged.

## 7. Where each topic lives

| topic | sections |
|---|---|
| origins: solved needs care, overfitting, grammar prior | 1-9 |
| curriculum problems, complexity ceilings, TSP, DTW | 10-19 |
| binary search ladder, skeleton catalogue, divergence engine | 20-30 |
| compositional gap, scoped conjunction, bloat, redundancy | 31-37 |
| combinator algebra and its promotion | 38-48, 52-56 |
| TSP guard by enumeration, soundness, metamorphic scaling | 41, 43, 44 |
| sorting diversity | 45, 46, 92-98, 118 |
| skeleton replacement by generic pieces | 56-58, 99-109 |
| shape vs fills (enumeration, SMT) | 63-69 |
| the stepwise promoted pieces | 70-91, 100-115 |
| external held-out sets and diagnosis | 114, 116 |
| restarts, confirm-reseed, cost-profile novelty | 94-98 |
| open specification and interval shells | 119-124 |
| comparison with published work (`docs/literature.md`), PSB2 (results: 4/180 held-out solved) | 126 |
| cleanup | 125-125c |

The per-section list with chunk and line is `docs/findings/INDEX.md`.
