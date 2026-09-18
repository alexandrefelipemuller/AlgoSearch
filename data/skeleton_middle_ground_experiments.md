# Skeleton middle-ground experiments

Baseline commit: `a97dc03` (TSP problem + brute-force + branch-and-bound
skeletons, branch-and-bound backtrack bug fixed and verified).

## Motivation

Every skeleton in this project so far is a binary, all-or-nothing prior:
either the generator hands the search a complete, already-correct
algorithm (found in tens to hundreds of candidates, then the beam
collapses onto copies of it with zero further exploration), or there is
no skeleton and the free/generic grammar gets essentially no traction on
anything past simple accumulator-shaped problems (confirmed directly for
TSP: 30 CPU-minutes at num-vars=80/candidates=20000 never got past V=2).

Going from the TSP brute-force skeleton to the branch-and-bound skeleton
required a human (me) to hand-author an entire second, structurally
different algorithm. That is a real, felt ceiling: skeleton *vocabulary*
only grows by a human writing another one. The question worth testing
before accepting that as permanent: was hand-authoring the only way to
reach branch-and-bound's result, or could some existing search mechanism
(crossover, mutation, a missing language primitive, more granular
building blocks) have gotten there -- or partway there -- without a human
writing that specific algorithm?

Four candidate answers, independent of each other, meant to be tested
separately before deciding whether/how to combine them.

## Test 1: crossover recombination with existing skeletons (no new code)

**Hypothesis:** the "running best, guard the update" idiom
(`if (x < best) { best = x }`) already exists in this project's grammar,
just embedded inside unrelated skeletons (SelectionSort's min-tracking,
ThreeSum's target check, TSP brute-force's own best-tracking). If
`crossoverRate` is raised so subtree recombination actually happens at a
meaningful rate, could grafting fragments from these into a TSP
brute-force-seeded population produce something pruning-shaped, without
anyone writing `genTspBranchAndBoundSkeletonBlock`?

**Method:** run TSP search with the branch-and-bound skeleton *disabled*
(`ALGOSEARCH_FORCE_SKELETON=tspbruteforce`, so only brute-force is ever
injected wholesale) but `--crossover-rate` swept across a few values
(0.0 baseline, then increasing), long enough budget to let recombination
actually occur across many generations. Compare: does held-out
correctness ever improve over pure brute force's timing profile at a
fixed V? Does anything resembling an early-continue/guard structure
appear in the winning candidate's AST at all, even if it doesn't help?

**What would count as a real result:** a generalizing candidate whose
per-instance step count or wall-clock time at large V is measurably
below pure brute force's, WITHOUT the branch-and-bound skeleton ever
being drawn. Absence of any such candidate over a real budget is itself
the answer (crossover-with-existing-vocabulary isn't sufficient here).

### Result (2026-09-17): negative

Ran with `ALGOSEARCH_DISABLE_SKELETON=tspbranchandbound` (so brute force
and every other pre-existing skeleton, e.g. SelectionSort/ThreeSum's
guarded running-best idiom, stayed available for crossover), sweeping
`--crossover-rate` in {0.0, 0.3, 0.6}, `--max-n 8`, 90s budget each,
seed=42:

- All three runs found and confirmed generalizing candidates at the same
  size range as plain brute force (237-267 nodes) through N=4-7
  (`confirmed_levels` 4-5), then stalled at N=8 (crossover-rate 0.0 and
  0.3) without confirming further within budget.
- At crossover-rate=0.3, the search DID find smaller candidates at N=8
  (size shrinking 258->135->133 across generations) -- but these were
  INCOMPLETE, not faster-and-correct: they scored only 12/16, i.e.
  actually wrong on some structural instances, not a leaner working
  algorithm. Smaller-and-broken, not smaller-and-smarter.
- No run at any crossover rate produced a candidate showing pruning-like
  behavior (no early-exit/guard structure appeared anywhere in a
  correct candidate beyond what plain brute force already has, i.e. the
  final `if (cost < best)` at the very end of each full permutation).

**Conclusion:** crossover with the existing skeleton vocabulary, even
pushed to a high rate (0.6), did not produce anything resembling
branch-and-bound-style pruning within this budget. This is consistent
with the structural argument made above: Heap's-algorithm brute force
has no partial-path state to prune ON in the first place (it always
commits to full permutations via swaps before ever computing a cost), so
grafting a guarded-comparison idiom from elsewhere can at best change
which complete permutation's cost gets kept, never skip exploring one --
there's nowhere for a spliced-in guard to earn its keep as a real prune.
Test 1 alone does not close the gap; the branch-and-bound skeleton (or a
comparable enumeration-shape change) still looks necessary for this
specific case. Worth revisiting on a problem whose brute-force skeleton
DOES have exploitable partial state (unlike TSP's Heap's-algorithm
enumeration) before generalizing this negative result too far.

## Test 2: "scaffold with a hole" -- partial skeleton

**Hypothesis:** the hard-to-discover part of branch-and-bound is the
*iteration shape* (explicit choice-stack DFS, incremental partial-cost
bookkeeping) -- extremely unlikely to emerge from unbiased random
generation. The *insight* part (the specific pruning comparison) is a
single, local, cheaply-searchable choice once the shape exists. If we
hand the search the shape but leave the pruning condition as an ordinary
generated expression (or omit it, or start it as `if (true)`/no-op),
does search reliably discover a working, useful pruning condition on its
own?

**Method:** a third skeleton, `genTspBranchAndBoundScaffoldBlock` (or a
flag on the existing one), identical to the branch-and-bound skeleton
except the `if (cost < best)` guard around `notPrunedBranch` is replaced
by a generated placeholder expression (drawn from the normal
`genValueExpr`/comparison grammar, using the same in-scope variables)
rather than the hand-written comparison. Run search, see what fraction
of runs (a) stay correct at all, since a wrong guard can break
correctness outright, not just performance, and (b) among the correct
ones, how many converge on something equivalent to `cost < best`.

**What would count as a real result:** the search reliably (most seeds)
converges on a correct, pruning-shaped guard on its own, at a reasonable
budget -- demonstrating the *shape* is the expensive part to hand over,
not the specific comparison.

### Result (2026-09-17): negative, and mechanistically clear why

Implemented as a `guardAsHole` parameter on `genTspBranchAndBoundSkeletonBlock`
(no code duplication): when true, the one line encoding `cost < best` is
replaced by `genCondExpr` (the ordinary random-condition grammar, same
in-scope variable pool). Registered as a separate skeleton name
(`tspbbscaffold`) so it can be isolated via `ALGOSEARCH_FORCE_SKELETON`
without touching the real branch-and-bound skeleton.

Ran forced-skeleton searches across 7 seeds (V up to 5, `--max-n 5`,
candidates=500, beam=200): **every seed converged to a correct,
generalizing candidate quickly** (held-out 8/8, `confirmed_levels` 4 in
the seed inspected in depth) -- so the DFS/choice-stack SHAPE really is
learnable/keepable by the search once handed over, confirming half the
hypothesis. But inspecting what filled the hole in each case:

- seed=7's converged guard: `v7 >= v11`, i.e. `mBase >= chBase` -- two
  CONSTANT offsets derived from `nv` (`1+4*nv` vs `1+2*nv`). Since
  `mBase > chBase` for every V >= 1, this is a **tautology**: always
  true, never prunes anything. Functionally identical to plain brute
  force wearing the DFS shape instead of Heap's permutation swaps --
  correct, zero speedup.
- Other seeds (1, 2, 3, 5, 11, 22) converged to similarly guard-shaped
  but equally vacuous conditions: small-integer literal comparisons
  against role variables whose value happens to stay in a narrow range
  for the tiny training instances (`level <= 1`, `size != 2`, etc.) --
  same pattern, not `cost`/`best` based.
- A separate, single **un-evolved** random draw (no beam search
  selection, just one `generateRandomProgram` call) landed on
  `(nv + pcBase + chBase) < c` -- a sum of large constant offsets
  compared against a small candidate-vertex index, almost always FALSE.
  This over-prunes almost the entire tree: measured at V=6-9, it ran
  100-1000x FASTER than real branch-and-bound (0.1-0.8ms vs 0.8-160ms)
  but was wrong on 7-11 of 16 instances at every V tested -- fast and
  broken, the mirror image of the safe-and-useless outcome above.

**Conclusion:** the shape alone is learnable, but evolution consistently
resolves the hole toward whichever extreme keeps CORRECTNESS at the
tested training sizes -- either a safe tautology (kept, no speedup) or,
left unselected, a reckless over-prune (would be rejected the moment it's
scored, since it fails outright on the same small training set). Neither
extreme is genuine pruning. This validates Test 4's hypothesis from the
opposite direction: the bottleneck isn't really "cost < best is a needle
in a haystack of possible expressions" (a purely combinatorial framing) --
it's that **nothing in the fitness signal rewards being fast**, so
there's no gradient pointing from "safe tautology, correct, slow" toward
"selective guard, correct, fast." A safe tautology already scores 100%
on every training instance the search checks; a real prune has no way to
earn extra credit for also being faster, so evolution has no reason to
keep drifting past the first correct (if useless) resting point it finds.

**Combined with Test 4, a real actionable hypothesis emerges:** these two
"shape but no insight" experiments both stall for the same underlying
reason, not two unrelated dead ends. If true, the fix isn't a smarter
hole/shape design -- it's giving the fitness function a speed-aware
signal (e.g. reward completing well under the step budget, not just
completing correctly) so a genuinely faster, still-correct candidate has
somewhere to climb toward. Worth testing directly before trying Test 3's
much larger re-architecture.

## Follow-up: speed-aware fitness signal (2026-09-17)

**Implemented:** `SearchConfig::speedBonusWeight` (0.0 default, CLI
`--speed-bonus-weight`). Adds `speedBonusWeight * (1 - steps_used/stepLimit)`
to `fitness` per SUCCESS instance -- never touches `score` (promotion/
labeling stays exactly as before), only breaks ties among equally-correct
candidates in favor of the faster one. Wired `Interpreter::run`'s new
optional `stepsOut` param through `Problem::evaluateInto` into
`BeamSearch::scoreCandidates` (all optional/default-null, zero cost or
behavior change when unused). All 58 tests + a clean build pass with the
feature at its default (off).

### Result: still negative

Re-ran the `tspbbscaffold` hole experiment (Test 2) with
`--speed-bonus-weight 0.5`, `--max-n 9`, `--step-limit 1000000` (the
timeout-pressured regime Test 4 flagged as the one place speed SHOULD
matter for fitness), seed=7:

- N=2-7: confirmed generalizing (`confirmed_levels` 6, held-out 8/8), but
  the converged guard is `c <= mBase` (`v7 <= v5` in that run's variable
  labeling) -- `c` is always a tiny vertex index (0..V-1), `mBase` is
  always a large constant offset (`1+4*nv`) that grows with V. Still a
  tautology, structurally identical in spirit to the pre-speed-bonus
  result (a different constant relationship, same vacuous shape).
- N=8: a NEW, different candidate took over (`confirmed_levels` reset to
  1, i.e. it doesn't hold at N=8's own lookahead), scoring 16/16 on
  training but only 7/8 held-out -- i.e. actually WRONG on an
  out-of-distribution case, not a faster correct algorithm. Its guard,
  `(w + cost) != (uBase - used[size])`, is essentially noise: it passed
  this level's specific 16 training instances by coincidence, not
  because it encodes anything resembling a real comparison.

**Conclusion:** a speed bonus at this weight/budget did not change the
outcome. Evolution still resolves the hole toward either a safe
tautology (at low N, where nothing is under real time pressure yet) or a
broken-but-lucky condition (at N=8, precisely where pressure exists) --
never toward a genuinely correct AND faster guard. This suggests the
working hypothesis from Test 2 (no speed signal => no gradient toward
pruning) was necessary but not sufficient to explain the failure, or the
weight/budget tested here still isn't enough to overcome it. A plausible
deeper reason: even with a speed gradient, `cost < best` sits in a very
narrow corridor of expression-space -- almost every single-mutation
neighbor of a tautology is either still vacuous or actively wrong, so
there's no smooth path for local mutation to climb even with the right
incentive pointing the way; discovering the exact right comparison still
needs something closer to "get lucky in one shot" than "gradually
improve."

**Not concluded yet, still open:** whether a much higher
`speedBonusWeight`, many more generations, or a narrower/more targeted
mutation operator (one that specifically tries swapping comparison
operators/operands on an EXISTING guard rather than regenerating a whole
new random expression) would do better. Given three independent
approaches (Tests 1, 2+speed-bonus, 4) have now all failed to produce
real pruning without a human writing the branch-and-bound skeleton
directly, Test 3's larger re-architecture (or simply accepting that this
specific capability needs a hand-authored skeleton, at least for now) is
the next honest fork in the road.

## Test 4: missing language primitive, not missing algorithm

(Moved up and run before Test 2 -- implementation was straightforward and
answers the more general "is the ceiling vocabulary or expressiveness"
question directly, matching the reframing above.)

**Implemented:** `Continue`/`Break` as new Mini-C statement kinds (AST +
interpreter + pretty-printer), gated by a new opt-in `GenConfig::
controlFlowBias` (0.0 default, CLI `--control-flow-bias`) -- only ever
emitted directly inside a while loop's reach (its own body or an if
nested inside it; `insideWhile` is now threaded through If generation
instead of being hard-reset to false, so `if (cond) continue;` -- the
shape pruning actually needs -- is reachable). An escaped continue/break
(past every enclosing loop) is scored as a runtime error, not a silent
success. Verified under ASan/UBSan and against all 58 existing tests
(unaffected, since the feature defaults off) before running the
experiment.

### Result (2026-09-17): negative at this budget

Ran TSP with `ALGOSEARCH_DISABLE_SKELETON=tspbranchandbound` (brute force
and every other skeleton still available) and `--control-flow-bias 0.15`,
`--max-n 8`, 300s budget, seed=42:

- Confirmed generalizing candidates at N=2-4 and N=5-7 (`confirmed_levels`
  3 each), sizes 262-266 nodes -- statistically indistinguishable from
  plain brute force's usual size range (237-267 in earlier baseline
  runs). Got stuck at N=8 (12/16, many timeouts) for the rest of the
  budget, same stuck-point plain brute force alone already shows.
- `grep -c "continue;\|break;"` across the entire run's output: **0**.
  The new primitive never appeared in any winning candidate at all, let
  alone in a pruning-shaped position.

**Why this might be the more informative failure than Test 1's:** N=8 is
exactly where brute force starts losing correctness score to timeouts
(12/16, not 16/16) -- i.e. this is precisely the regime where a
pruning-capable candidate SHOULD have a fitness edge (it would keep
completing in time while plain brute force starts timing out). Even with
selection pressure pointing the right direction, `continue`/`break`
existing as ordinary generatable statements wasn't enough to get
discovered/kept within this budget.

**Working hypothesis for why, worth testing before writing this off:**
the fitness function here is purely correctness-based (score = fraction
of instances solved right); it has NO direct notion of "faster," only
"times out or doesn't" as a binary. A pruning candidate only wins once
it's already good enough to flip a timeout into a completion -- a single
lucky insertion of `continue` in the right spot, guarded by the right
condition, is a large, specific, multi-part mutation (right loop, right
guard expression, right consequence) for unbiased mutation to stumble
into, especially competing against an already-100%-correct-at-lower-N
brute force candidate that has no local incentive to change at all.
Unlike Test 1 (recombining existing idioms), this suggests the ceiling
here is jointly: (a) needing several correlated pieces at once
(loop-agnostic guard content, hitting the very rare V=8 regime where it
would even matter), not myopic, one-mutation-at-a-time discovery, and
(b) fitness being blind to speed except at the sharp cliff where timeout
flips a score bit.

**Not yet tried, worth doing before calling Test 4 fully negative:**
higher `controlFlowBias`, a much longer budget specifically AT the
regime where brute force is already timing out (start search at N=8
directly with a seed candidate, rather than growing up from N=1), or
combining with Test 2's scaffold (hand over the loop/array shape, leave
ONLY the guard+continue as the discoverable part, rather than asking
mutation to invent both the shape and the primitive's placement from
scratch).

## Test 3: modular / composable skeleton pieces

**Hypothesis:** monolithic skeletons (one function emits one complete,
internally-consistent program) are themselves the limiting factor --
they can't recombine with each other because each owns its own closed
set of variable roles. Breaking skeletons into independent, smaller
primitives with a shared variable-role interface (e.g. "array-zeroing
init loop", "running-best guarded update", "explicit stack-based DFS
scaffold") that crossover/mutation can freely mix would be a structurally
different, more compositional grammar -- closer to genetic programming
with automatically-defined functions/modules than today's "one skeleton,
one complete algorithm" scheme.

**Method:** this is the most invasive option -- it requires re-architecting
how a skeleton claims variables and how partial pieces compose safely
(e.g. a shared naming/role convention so an "init loop" piece and a
"DFS scaffold" piece agree on which variable is `nv`, which is `best`,
etc., without a human wiring them together by hand each time). Worth
scoping in detail only if Tests 1 and 2 both come back negative --
i.e. only if neither "recombine what already exists" nor "hand over the
shape, discover the insight" gets useful mileage.

## Test 4: missing language primitive, not missing algorithm

**Hypothesis:** this project already broke a ceiling once this way --
`gcd`/`is_prime` were stuck until native `Mul`/`Div`/`Mod` were added to
Mini-C, after which the *free* search solved them with no skeleton at
all. Branch-and-bound's pruning is structurally an early-exit/`continue`
out of the current loop iteration -- a control-flow primitive Mini-C does
not have (`if` only has body/elseBody, no `break`/`continue`). Maybe the
real ceiling for "any problem needing pruning" isn't "needs a bespoke
skeleton per problem" but "needs one missing language primitive,
generically" -- the same way one arithmetic extension unlocked multiple
problems at once instead of needing a skeleton for each.

**Method:** add a `Continue` (or `Break`) statement kind to Mini-C
(interpreter + AST + generator, gated the same way `enableArithmeticExt`
gates Mul/Div/Mod so nothing existing is affected by default), then test
whether free/generic search on TSP (still no branch-and-bound skeleton,
brute-force skeleton free to compete but not forced) makes any
measurable progress toward pruning-shaped behavior once `continue`
exists as an ordinary generatable statement. This is a bigger, riskier
change (new interpreter semantics) but has the highest potential payoff:
if true, it generalizes to every future problem needing pruning/early
exit, not just TSP.

## Suggested order

1. Test 1 first -- purely a search-config experiment, zero new code,
   reuses everything already built.
2. Test 4's hypothesis is worth reasoning about/scoping in parallel since
   it's the highest-leverage if true, but the implementation (new
   interpreter statement kind) is nontrivial -- sequence after Test 1's
   result is in.
3. Test 2 next -- one new skeleton variant, moderate effort.
4. Test 3 only if 1, 2, and 4 all fail to close the gap -- it's a real
   grammar re-architecture, not a quick experiment.

Update this file with each test's actual result before moving to the
next, so the comparison across all four is honest and complete rather
than stopping at the first thing that looks promising.

## Reframing (2026-09-17): the goal is general capability, not reproducing branch-and-bound specifically

None of these tests should be judged by "did it reproduce
branch-and-bound's exact pruning behavior for TSP." That's one
convenient, freshly-understood example, not the target. The actual goal
is: can the search's GENERAL capability (across whatever problem) go up
without a human hand-authoring a new dedicated skeleton for every new
capability? A test that fails to rediscover branch-and-bound but reveals
some other generalizable gain (faster convergence, smaller programs,
new idioms reusable elsewhere, etc.) is still a meaningful positive
result and should be recorded as such, not written off because it
missed the specific TSP target.

## Scope: this is not a TSP-only question

TSP (brute-force -> branch-and-bound) is the working example because it's
the freshest, cleanest before/after case, but the actual question is
about the skeleton *mechanism* in this project generally -- every problem
here (sorting/heapsort, is_prime/gcd's native-mod skeletons,
graph_reachability, shortest_path, three_sum, single_number, etc.) is a
reference point for whether "hand-author a new skeleton" is really the
only lever, or whether one of Tests 1/2/4 generalizes as a cheaper
alternative across problems, not just this one. Once a test produces a
result on TSP, spot-check at least one other problem with a known
skeleton-dependency story before treating the conclusion as general.

## Test 3 (scoped): expression-level point mutation

**Implemented, scoped down from the full modular-skeleton re-architecture
described above:** the real gap found while running Test 2 was that
`growProgram`'s only existing point mutation replaces an ENTIRE
if/while statement (condition + body + else-body) with a fresh one --
so any mutation that touches a guard's `if` also destroys whatever
correct logic is nested inside it (the placement/leaf/descend logic in
the branch-and-bound scaffold), making it almost always fatal and never
selected. Added `GenConfig::exprMutationRate` (0.0 default, CLI
`--expr-mutation-rate`): with this probability, `growProgram` now ALSO
regenerates just one existing if/while's `cond` in place via a new
`collectCondSites` helper, leaving body/else-body untouched. All 58
tests + a clean ASan/UBSan build pass with the feature at its default.

### Result (2026-09-17): still negative on the core question, one positive side-effect

Re-ran the `tspbbscaffold` hole experiment combined with BOTH
`--speed-bonus-weight 0.5` and `--expr-mutation-rate 0.5`, `--max-n 9`,
`--step-limit 1000000`, seed=7:

- N=3-7: confirmed (`confirmed_levels` 5, held-out 8/8), guard = `(3 ^ v6) != 2` -- a bitwise XOR against constants, still not referencing
  `cost`/`best` at all.
- N=8: **held-out 8/8 this time** (an improvement -- the equivalent run
  without expr-mutation had a broken 7/8 candidate here). Guard =
  `((best - best) + best) != nv`, which simplifies to `best != nv` --
  still an almost-always-true near-tautology (best starts at 100000 and
  nv is tiny), just a more convoluted way to express one.

**Conclusion:** fine-grained mutation measurably diversified WHICH
vacuous guard the search lands on (bitwise tricks, self-cancelling
arithmetic) and incidentally improved held-out robustness at N=8, but it
still never converged toward anything resembling `cost < best`. Being
able to tweak a guard without destroying its body was a real, confirmed
gap (worth keeping as a general-purpose operator regardless), but fixing
it alone wasn't sufficient to cross into genuine pruning within this
budget either.

## Overall status across all four tests

Four independent approaches to growing this capability WITHOUT a human
hand-authoring the branch-and-bound skeleton directly, all tested on the
same TSP pruning case:

1. **Crossover with existing skeleton vocabulary** -- negative (no
   partial-path state to prune on in brute force's enumeration shape).
2. **Scaffold with a hole** (hand over the DFS shape, leave the guard
   open) -- negative (evolution settles on a vacuous tautology; shape IS
   learnable, insight is not).
3. **Speed-aware fitness signal** -- negative (didn't change the
   outcome; the correct comparison likely sits in too narrow a corridor
   of expression-space for the existing mutation granularity).
4. **Expression-level point mutation** (fixing the granularity gap Test
   3's investigation surfaced) + **missing language primitive**
   (`continue`/`break`) -- both negative on the core question, though
   the mutation fix improved robustness as a side effect and is worth
   keeping.

**Working conclusion:** for a capability this qualitatively different
from what the search already has priors for (an enumeration algorithm
with an early-exit condition based on a running best-so-far, evaluated
against a partial/incomplete solution rather than a complete one), none
of the four levers tested -- recombination, partial scaffolding, a speed
signal, or finer mutation granularity -- got the free search to
rediscover it within the budgets tried here. This doesn't prove no
combination or larger budget ever could, but it's a real, reproducible
ceiling worth respecting rather than assuming away: for now, a
genuinely new algorithmic SHAPE (not a variant of one already in the
grammar) still needs a human to write it once, the way
`genTspBranchAndBoundSkeletonBlock` was written. The useful infra built
along the way (`ALGOSEARCH_DISABLE_SKELETON`, `continue`/`break`,
`speedBonusWeight`, `exprMutationRate`) stays available for the next
capability gap this project runs into, where the answer might differ.
