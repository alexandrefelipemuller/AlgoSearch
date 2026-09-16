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
   `while`, `swap`, and integer expressions (`+`, `-`, and native
   comparisons `<`, `>`, `==`, `<=`, `>=`, `!=`).
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
make test       # builds and runs ./algosearch_tests (49 unit/integration tests)
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

## Results: all 10 problems, one modest-budget sweep

The table below is from a single sweep across every problem this project
currently supports, at the **same modest search budget** for each
(`--beam-width 500 --candidates 1500 --max-program-size 150
--benchmark-seeds 5`, current grammar defaults —
`loopAccumulatorBias=0.4`, native `Le`/`Ge`/`Ne`). Every row is a solution
that survived `--benchmark`'s independent adversarial re-check, not just
the training score. "Time to generate" is the search wall-clock time per
seed (mean across the 5 benchmark seeds) on an 8-core machine — how long
it took to *find* the solution, not how fast the discovered algorithm
runs.

| Problem | Result | Complexity | Time to generate (avg/seed) |
|---|---|---|---|
| `sorting` | 3 solutions confirmed | O(n²) | ~0.09s |
| `min` | 2 solutions confirmed | O(n) | ~0.09s |
| `max` | 4 solutions confirmed | O(n) | ~0.08s |
| `sum` | 3 solutions confirmed | O(n) | ~0.09s |
| `contains` | 4 solutions confirmed | O(n) | ~0.09s |
| `contains_ordered` | 4 solutions confirmed | O(n) | ~0.10s |
| `gcd` | subtractive Euclidean GCD, confirmed through level 20 | O(n) | ~0.15s |
| `is_prime` | 6 confirmed, 3 correctly rejected by the adversarial check | O(n) | ~8.1s |
| `fibonacci` | 3 solutions confirmed | O(n) | ~0.08s |
| `factorial` | 5 solutions confirmed (nested-loop multiply-by-repeated-addition) | O(n²) | ~49.6s |

Two results worth calling out:

- **`gcd`** converges on genuine subtractive Euclidean GCD — a clean,
  short, textbook algorithm, not an overfit blob:
  ```
  v2 = arr[0]; v1 = arr[1];
  while ((v2<v1)+(v2>v1)) {
    if (v2>v1) v2 = v2-v1; else v1 = v1-v2;
  }
  arr[0] = v2;
  ```
  This is notable because a documented "wall" (searches getting stuck at
  63-79/100 on `gcd`/`mod` with the skeleton bias *disabled*, six
  different selection/diversity mechanisms all failing identically) does
  **not** appear here, with the skeleton bias at its normal default — see
  `FINDINGS.md` section 9 for the full story of when the wall does and
  doesn't show up.
- **`factorial`** looked stuck in an earlier run (timed out at 240s,
  plateaued 43/45) purely from CPU contention with other problems running
  in the same sequential batch — rerun in isolation it converges in
  under a minute. The remaining 2/45 "failures" at its hardest training
  level aren't a search failure at all: `factorial(27)` overflows a
  32-bit `int`, so the discovered algorithm is actually correct, just
  bounded by the interpreter's integer type.

See `FINDINGS.md` for the full experimental history (grammar bias sweeps,
diversity/crossover/niching A/B tests, the native-comparison-operator
change, and the honest negative results along the way) and
`scripts/run_full_battery_modest.sh` to reproduce this sweep.
