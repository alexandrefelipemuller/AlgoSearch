#include "search/ProgramGenerator.hpp"

#include <cstdlib>
#include <cstring>
#include <functional>
#include <vector>

namespace algosearch {

namespace {

ExprPtr genValueExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    if (depthBudget <= 0 || rng.chance(0.35)) {
        // leaf
        if (rng.chance(0.5)) {
            return makeIntConst(rng.uniformInt(0, cfg.maxConst));
        }
        return makeVar(rng.uniformInt(0, cfg.numVars - 1));
    }
    // Bug found while investigating why GcdProblem/IsPrimeProblem never
    // compose a solution unaided: case 0 and the default case here used
    // to BOTH produce ArrayAccess, silently giving it 50% weight against
    // 25% each for Add/Sub -- fine for array-heavy problems (min/max/sum),
    // but it actively starves magnitude problems (gcd, is_prime,
    // fibonacci, factorial) whose array holds only 1-2 elements, where
    // almost all real computation has to happen via scalar-to-scalar
    // arithmetic. `Sub(Var, Var)` -- confirmed empirically as the exact
    // missing piece behind GcdProblem's 72/100 plateau (see the
    // LANDSCAPE diagnostic in BeamSearch.cpp) -- was accidentally rarer
    // than it needed to be. Fourth branch now returns a direct scalar Var
    // (not recursing further), giving genuine 25% weight to "just a
    // variable" at every non-leaf level, not only when the leaf branch
    // above happens to roll one.
    // Arithmetic/bitwise extension (FINDINGS.md section 16, added for the
    // Graph/BitManip problems): a small, always-on 8% substitution instead
    // of folding these into the 4-way choice below, which would dilute the
    // carefully-tuned Add/Sub/ArrayAccess/Var split documented above (the
    // exact kind of dilution the Sub(Var,Var) bug was about). Universal for
    // every problem, same "single trigger, no problem-specific knob"
    // principle as loopAccumulatorBias -- not gated per-problem, since a
    // per-problem CLI flag would reveal which algorithm a problem needs
    // (see GenConfig's loopAccumulatorBias comment). Mul/Div/Mod/BitAnd/
    // BitOr/BitXor/Shl/Shr are otherwise unreachable by the free grammar.
    if (depthBudget > 0 && rng.chance(0.08)) {
        static const ExprKind extKinds[] = {ExprKind::Mul,    ExprKind::Div,    ExprKind::Mod,   ExprKind::BitAnd,
                                             ExprKind::BitOr, ExprKind::BitXor, ExprKind::Shl,   ExprKind::Shr};
        ExprKind k = extKinds[rng.uniformInt(0, 7)];
        return makeBinOp(k, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
    }

    int choice = rng.uniformInt(0, 3);
    switch (choice) {
        case 0:
            return makeArrayAccess(genValueExpr(rng, depthBudget - 1, cfg));
        case 1:
            return makeBinOp(ExprKind::Add, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
        case 2:
            return makeBinOp(ExprKind::Sub, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
        default:
            return makeVar(rng.uniformInt(0, cfg.numVars - 1));
    }
}

// Le/Ge/Ne are native ExprKinds (see ast/Expr.hpp), not emulated compounds.
// Sampled uniformly against Lt/Gt/Eq so every comparison is one grammar
// step away from any other, instead of forcing >=/<= through a fragile
// off-by-one rewrite (e.g. Gt(a, Sub(b,1))) or != through a compound
// (Lt(x,y)+Gt(x,y)).
ExprPtr genCondExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    ExprKind kinds[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    ExprKind k = kinds[rng.uniformInt(0, 5)];
    return makeCompare(k, genValueExpr(rng, depthBudget, cfg), genValueExpr(rng, depthBudget, cfg));
}

std::vector<StmtPtr> genBlockStmts(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, int maxStmts,
                                    bool insideIf = false, bool insideWhile = false);

// Which of the five accumulator shapes a genAccumulatorBodyStmtOfKind call
// produced. Sum/Min/Max/Membership accumulate into the scalar `acc`
// variable and need a write-back into arr[0] after the loop (the
// interpreter's output is the array, not scalars -- see
// genCountingLoopBlock); Swap already writes its result straight into the
// array inside the loop body and must NOT get an extra write-back (it
// would silently clobber a correct result with acc's stale/unrelated
// value).
enum class AccumKind { Sum, Min, Max, Membership, Swap, CountIf, Xor };

bool accumKindNeedsWriteBack(AccumKind k) {
    return k == AccumKind::Sum || k == AccumKind::Min || k == AccumKind::Max || k == AccumKind::Membership ||
           k == AccumKind::CountIf || k == AccumKind::Xor;
}

// One canonical accumulation body for the counting-loop skeleton below.
// `vi` is the loop index variable, `acc` a second variable seeded before
// the loop. `targetVar` is only used by Membership (see below); pass -1
// for every other kind. All five shapes touch arr[vi] -- the one thing
// every array algorithm (min/max/sum/contains/sort) actually needs a loop
// for.
StmtPtr genAccumulatorBodyStmtOfKind(int vi, int acc, AccumKind kind, int targetVar, const GenConfig& cfg) {
    switch (kind) {
        case AccumKind::Sum:  // running sum: acc = acc + arr[vi]
            return makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeArrayAccess(makeVar(vi))));
        case AccumKind::Min:  // running min: if (arr[vi] < acc) acc = arr[vi]
        {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeArrayAccess(makeVar(vi))));
            return makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(makeVar(vi)), makeVar(acc)), std::move(then));
        }
        case AccumKind::Max:  // running max: if (arr[vi] > acc) acc = arr[vi]
        {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeArrayAccess(makeVar(vi))));
            return makeIf(makeCompare(ExprKind::Gt, makeArrayAccess(makeVar(vi)), makeVar(acc)), std::move(then));
        }
        case AccumKind::Membership:  // if (arr[vi] == target) acc = 1
        {
            // Compares against a scalar `target` (captured from arr[0]
            // BEFORE the loop starts, by genCountingLoopBlock) rather than
            // arr[0] directly, and writes the found-flag into `acc`
            // (write-back to arr[0] happens once, after the loop) --
            // comparing directly against arr[0] here would be comparing
            // against a slot the loop itself scans over and that a match
            // immediately overwrites, self-corrupting on the very first
            // iteration regardless of the real answer. See the long
            // comment in genCountingLoopBlock for the full story.
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeIntConst(1)));
            return makeIf(makeCompare(ExprKind::Eq, makeArrayAccess(makeVar(vi)), makeVar(targetVar)), std::move(then));
        }
        case AccumKind::CountIf:  // counter under a condition: if (arr[vi] > threshold) acc = acc + 1
        {
            // Same captured-scalar convention as Membership (threshold in
            // `targetVar`, captured from arr[0] before the loop, vi
            // starting at 1 to skip that slot) -- the piece missing for
            // CountProblem before this: every other AccumKind either sets
            // a flag (Membership) or tracks a running aggregate of the
            // ELEMENT itself (Sum/Min/Max), none of them increment a
            // counter gated by a comparison against something else.
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeIntConst(1))));
            return makeIf(makeCompare(ExprKind::Gt, makeArrayAccess(makeVar(vi)), makeVar(targetVar)), std::move(then));
        }
        case AccumKind::Xor:  // running xor: acc = acc ^ arr[vi] -- the "Single Number" bit trick
                              // (see SingleNumberProblem): every value that appears an even
                              // number of times XORs away to 0, leaving only the odd one out.
                              // Needs the BitXor primitive (GenConfig arithmetic/bitwise
                              // extension, FINDINGS.md section 16) -- unreachable before it.
            return makeAssign(acc, makeBinOp(ExprKind::BitXor, makeVar(acc), makeArrayAccess(makeVar(vi))));
        default:  // adjacent compare-and-swap: if (arr[vi] > arr[vi+1]) swap(arr[vi], arr[vi+1])
        {
            auto next = makeBinOp(ExprKind::Add, makeVar(vi), makeIntConst(1));
            auto cond = makeCompare(ExprKind::Gt, makeArrayAccess(makeVar(vi)), makeArrayAccess(next->clone()));
            std::vector<StmtPtr> then;
            then.push_back(makeSwap(makeVar(vi), next->clone()));
            (void)cfg;
            return makeIf(std::move(cond), std::move(then));
        }
    }
}

// The canonical "counting loop" skeleton every array algorithm needs:
// an index that starts at 0, is bounds-checked against n (var 0, by
// interpreter convention initialized to the array length), and
// increments exactly once per iteration -- wrapped as a single Block so
// it can be generated/inserted anywhere a single statement is expected.
StmtPtr genCountingLoopBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    // Reserve var 0 (the array length) and pick two distinct scalar
    // variables for the index and accumulator from the rest.
    int vi = 1 + rng.uniformInt(0, cfg.numVars - 2);
    int acc = 1 + rng.uniformInt(0, cfg.numVars - 2);
    if (acc == vi) acc = 1 + (vi % (cfg.numVars - 1));

    // Kind is decided up front (not per-call) so init, body, and write-back
    // all agree on what's being computed -- bugs, found by investigating
    // why SumArrayProblem sometimes never converged even with
    // loopAccumulatorBias saturated at 1.0 and 20000 sampled programs,
    // fixed together here:
    //  1. Init was a random 50/50 coin flip between acc=0 and acc=arr[0]
    //     regardless of kind, but that choice is only ever correct for ONE
    //     of Sum (needs 0) vs Min/Max (needs arr[0] -- 0 is not a valid
    //     sentinel unless every array value happens to be >=0/<=0) --
    //     halving the correctness rate even when everything else was
    //     right.
    //  2. Sum/Min/Max accumulate into the scalar `acc`, but the
    //     interpreter's output is the ARRAY (output[0]), not scalars --
    //     the loop itself never copied acc back into arr[0]. It only ever
    //     "worked" when a LATER, structurally unrelated random statement
    //     happened to also emit `arr[0] = acc` by coincidence (confirmed:
    //     MinArrayProblem's actual discovered solution has exactly that
    //     extra line, tacked on after the loop by luck, not by design).
    //     A direct probe generating 20000 programs with
    //     loopAccumulatorBias=1.0 found zero -- not rare, never -- correct
    //     standalone sum programs before this fix.
    //  3. Membership originally compared `arr[vi] == arr[0]` with `vi`
    //     starting at 0, same as every other kind -- but a target-style
    //     problem (ContainsProblem's convention: arr[0] = target,
    //     arr[1..] = haystack) treats index 0 as special, not ordinary
    //     data. On the very first iteration (vi=0) that compared arr[0] to
    //     itself, always true, and immediately overwrote arr[0]=1 --
    //     corrupting the target and reporting "found" unconditionally,
    //     regardless of whether it actually was. Membership now captures
    //     `target = arr[0]` into its own scalar before the loop and starts
    //     `vi` at 1 (skipping the target slot) instead of 0, exactly the
    //     kind of ordinary-data-vs-special-slot distinction only this one
    //     kind needs -- every other kind still starts at 0 as before.
    //     Needs one more scratch variable (target) than the other kinds,
    //     so Membership is only chosen when numVars leaves room for it.
    //     CountIf (counter under a condition -- the piece CountProblem
    //     needs, see genAccumulatorBodyStmtOfKind) reuses the exact same
    //     captured-target/vi-starts-at-1 convention as Membership, for the
    //     same reason: it also compares against a query slot at arr[0],
    //     not ordinary haystack data.
    bool canTargetKind = cfg.numVars >= 4;  // Membership and CountIf both need the extra captured-target scalar
    int kindCount = canTargetKind ? 7 : 5;  // + Membership, CountIf when there's room for the target scalar
    int draw = rng.uniformInt(0, kindCount - 1);
    AccumKind kind;
    switch (draw) {
        case 0: kind = AccumKind::Sum; break;
        case 1: kind = AccumKind::Min; break;
        case 2: kind = AccumKind::Max; break;
        case 3: kind = AccumKind::Swap; break;
        case 4: kind = AccumKind::Xor; break;
        case 5: kind = AccumKind::Membership; break;
        default: kind = AccumKind::CountIf; break;
    }
    bool needsArr0Init = (kind == AccumKind::Min || kind == AccumKind::Max);
    bool needsTarget = (kind == AccumKind::Membership || kind == AccumKind::CountIf);

    int targetVar = -1;
    auto root = makeBlock();
    if (needsTarget) {
        for (int cand = 1; cand < cfg.numVars; ++cand) {
            if (cand != vi && cand != acc) {
                targetVar = cand;
                break;
            }
        }
        // Membership captures the query as an EQUALITY target; CountIf
        // reuses the same slot as a comparison THRESHOLD instead --
        // different semantics, same "one scalar captured from arr[0]
        // before the loop starts" mechanism.
        root->body.push_back(makeAssign(targetVar, makeArrayAccess(makeIntConst(0))));  // target = arr[0]
    }
    root->body.push_back(makeAssign(acc, needsArr0Init ? makeArrayAccess(makeIntConst(0)) : makeIntConst(0)));
    root->body.push_back(makeAssign(vi, makeIntConst(needsTarget ? 1 : 0)));

    std::vector<StmtPtr> body;
    body.push_back(genAccumulatorBodyStmtOfKind(vi, acc, kind, targetVar, cfg));
    // A second statement of the SAME kind is redundant but harmless
    // (idempotent for min/max/membership, and correct-but-pointless for
    // sum, since subsequent statements at least stay consistent with the
    // first); mixing a different kind in the same loop body would corrupt
    // whichever one runs second, so kind is never re-rolled here.
    if (rng.chance(0.3)) body.push_back(genAccumulatorBodyStmtOfKind(vi, acc, kind, targetVar, cfg));
    body.push_back(makeAssign(vi, makeBinOp(ExprKind::Add, makeVar(vi), makeIntConst(1))));

    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(vi), makeVar(0)), std::move(body)));

    if (accumKindNeedsWriteBack(kind)) {
        root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(acc)));
    }
    return root;
}

// Dedicated nested skeleton for trial-division-by-subtraction primality
// testing -- see the loopAccumulatorBias comment in ProgramGenerator.hpp
// for the full rationale. Reserves var 0 (array length, unused here) and
// picks four distinct scalars: x (the value under test), d (candidate
// divisor), r (repeated-subtraction remainder), and sq (see below).
//
// Bounds the outer loop by `d*d <= x` (trial division only needs
// divisors up to sqrt(x): if x has no divisor <= sqrt(x), it has none
// above it either) instead of the naive `d < x` -- O(sqrt(n)) candidate
// divisors instead of O(n). Mini-C has no multiply primitive, so `d*d`
// can't be computed directly each round, but it doesn't need to be:
// `sq` tracks d^2 incrementally, using the identity (d+1)^2 = d^2 + 2d+1
// -- pure Add, updated once per outer iteration right before `d` itself
// increments, so `sq` is always exactly the square of the CURRENT `d`
// when the loop condition checks it. Needs one more scratch scalar than
// the naive O(n) version, hence numVars >= 5 (one tier up from the
// original's numVars >= 3).
StmtPtr genNestedDivisorLoopBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[4];
    int used = 0;
    while (used < 4) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int x = vars[0], d = vars[1], r = vars[2], sq = vars[3];

    auto root = makeBlock();
    root->body.push_back(makeAssign(x, makeArrayAccess(makeIntConst(0))));    // x = arr[0]
    root->body.push_back(makeAssignArray(makeIntConst(0), makeIntConst(1)));  // arr[0] = 1 (assume prime)
    {
        std::vector<StmtPtr> guardBody;
        guardBody.push_back(makeAssignArray(makeIntConst(0), makeIntConst(0)));
        root->body.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(x), makeIntConst(2)), std::move(guardBody)));
    }
    root->body.push_back(makeAssign(d, makeIntConst(2)));   // d = 2
    root->body.push_back(makeAssign(sq, makeIntConst(4)));  // sq = 2*2  (d^2, kept in sync with d below)

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(r, makeVar(x)));  // r = x
    {
        std::vector<StmtPtr> innerBody;
        innerBody.push_back(makeAssign(r, makeBinOp(ExprKind::Sub, makeVar(r), makeVar(d))));
        loopsUsed++;
        outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(d), makeVar(r)), std::move(innerBody)));
    }
    {
        std::vector<StmtPtr> foundBody;
        foundBody.push_back(makeAssignArray(makeIntConst(0), makeIntConst(0)));
        outerBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(r), makeVar(d)), std::move(foundBody)));
    }
    // sq = sq + 2*d + 1 (using CURRENT d, i.e. (d+1)^2 = d^2 + 2d + 1),
    // THEN d = d + 1 -- order matters: sq must advance to match the new
    // d exactly, using the old d's value one last time.
    outerBody.push_back(
        makeAssign(sq, makeBinOp(ExprKind::Add, makeVar(sq), makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeVar(d), makeVar(d)),
                                                                          makeIntConst(1)))));
    outerBody.push_back(makeAssign(d, makeBinOp(ExprKind::Add, makeVar(d), makeIntConst(1))));  // d = d + 1
    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Le, makeVar(sq), makeVar(x)), std::move(outerBody)));

    return root;
}

// Native-arithmetic sibling of genNestedDivisorLoopBlock, added for
// FINDINGS.md section 16 ("breaking the language ceiling"). Section 13
// found the sqrt-bound version above still costs O(x log x) overall
// because each candidate's divisibility check is repeated-subtraction
// (O(x/d) per candidate) -- the CANDIDATE COUNT shrank to O(sqrt(x)) but
// the PER-CANDIDATE COST didn't. With native Mul (for the `d*d <= x`
// bound, no more incremental-square tracking needed) and native Mod (for
// an O(1) `x mod d == 0` divisibility check), both halves are now O(1)
// per candidate, so the whole loop is genuinely O(sqrt(x)) total, not
// merely O(sqrt(x)) candidates each still doing linear work. Only needs 2
// scratch scalars (x, d) -- no remainder or incremental-square variable --
// and a single loop, so it's gated on nothing more than numVars >= 3,
// one tier BELOW the original (a direct consequence of the primitives
// doing the work the grammar used to have to spend variables/loops
// simulating). Kept as a separate skeleton alongside the original rather
// than replacing it, same "measurable A/B" reasoning as GcdModNative.
StmtPtr genNestedDivisorNativeLoopBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[2];
    int used = 0;
    while (used < 2) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int x = vars[0], d = vars[1];

    auto root = makeBlock();
    root->body.push_back(makeAssign(x, makeArrayAccess(makeIntConst(0))));    // x = arr[0]
    root->body.push_back(makeAssignArray(makeIntConst(0), makeIntConst(1)));  // arr[0] = 1 (assume prime)
    {
        std::vector<StmtPtr> guardBody;
        guardBody.push_back(makeAssignArray(makeIntConst(0), makeIntConst(0)));
        root->body.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(x), makeIntConst(2)), std::move(guardBody)));
    }
    root->body.push_back(makeAssign(d, makeIntConst(2)));  // d = 2

    std::vector<StmtPtr> outerBody;
    {
        std::vector<StmtPtr> foundBody;
        foundBody.push_back(makeAssignArray(makeIntConst(0), makeIntConst(0)));
        outerBody.push_back(
            makeIf(makeCompare(ExprKind::Eq, makeBinOp(ExprKind::Mod, makeVar(x), makeVar(d)), makeIntConst(0)), std::move(foundBody)));
    }
    outerBody.push_back(makeAssign(d, makeBinOp(ExprKind::Add, makeVar(d), makeIntConst(1))));  // d = d + 1
    loopsUsed++;
    root->body.push_back(
        makeWhile(makeCompare(ExprKind::Le, makeBinOp(ExprKind::Mul, makeVar(d), makeVar(d)), makeVar(x)), std::move(outerBody)));

    return root;
}

// Which single-loop scalar recurrence genRecurrenceSkeletonBlock builds.
// GcdEuclid and Fibonacci used to be two separate top-level Skeleton
// options; merged into one draw (see the "fewer skeletons" note on the
// Skeleton enum in genStmt) since both are "seed two scalars, update them
// with Add/Sub each iteration of a single loop, write the result back" --
// the same shape genCountingLoopBlock already uses for its five AccumKinds.
// They keep genuinely different bodies/conditions internally (subtractive
// alternation gated on inequality vs an additive recurrence gated on a
// counter) -- unifying the DRAW, not pretending the two algorithms are
// actually the same computation.
enum class RecurrenceKind { GcdSubtract, FibonacciAdd, GcdMod, GcdModNative };

// GcdSubtract: subtractive Euclidean GCD (a,b seeded from arr[0]/arr[1],
// loop while a != b subtracting the smaller from the larger, write back
// a) -- O(max(a,b)) steps in the worst case (e.g. gcd(n, 1) takes n
// subtractions). FibonacciAdd: iterative Fibonacci (a,b seeded 0,1, a
// third counter i looping while i < arr[0]; `b = a + b; a = b - a;`
// recovers the new `a` from the just-updated `b` and the OLD `a`, still
// available since it hasn't been overwritten yet, avoiding a third temp
// variable). GcdMod: the REAL fast Euclidean algorithm, `(a,b) = (b, a
// mod b)` repeated until b hits 0 -- O(log(min(a,b))) OUTER iterations
// (a Fibonacci-ratio bound, the classic result), each computing `a mod
// b` via its own repeated-subtraction inner loop since Mini-C has no
// divide primitive. Honest caveat, not swept under the rug: that inner
// mod step costs O(a/b) subtractions, so unlike a language with a native
// divide/modulo instruction, this does NOT guarantee O(log n) total
// primitive-operation count in the worst case (e.g. gcd(n, 2) still costs
// O(n) subtractions in that one mod step) -- what it guarantees is
// O(log n) *outer* iterations, and empirically much less total work than
// GcdSubtract whenever the two operands stay reasonably balanced across
// iterations, which --benchmark's curve fit can measure directly. Picked
// among whichever variants fit the current numVars/loop budget:
// FibonacciAdd needs one extra scratch scalar (the counter); GcdMod needs
// two extra (the mod loop's own remainder, plus reusing GcdSubtract's a/b
// slots) and a free nested-loop budget slot, being itself a 2-loop shape.
StmtPtr genRecurrenceSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed, bool canNestedLoop) {
    int a = 1 + rng.uniformInt(0, cfg.numVars - 2);
    int b = 1 + rng.uniformInt(0, cfg.numVars - 2);
    if (b == a) b = 1 + (a % (cfg.numVars - 1));

    RecurrenceKind options[4];
    int n = 0;
    options[n++] = RecurrenceKind::GcdSubtract;
    if (cfg.numVars >= 4) options[n++] = RecurrenceKind::FibonacciAdd;
    if (canNestedLoop && cfg.numVars >= 4) options[n++] = RecurrenceKind::GcdMod;
    // GcdModNative added for FINDINGS.md section 16 ("breaking the language
    // ceiling"): identical (a,b) = (b, a mod b) recurrence as GcdMod, but
    // using the new native Mod primitive for `a mod b` in O(1) instead of
    // GcdMod's repeated-subtraction inner loop -- a single loop, not two,
    // so it only needs cfg.numVars >= 4 (no canNestedLoop requirement).
    // Kept as a SEPARATE option rather than replacing GcdMod so the A/B
    // complexity comparison against the honest repeated-subtraction version
    // stays measurable.
    if (cfg.numVars >= 4) options[n++] = RecurrenceKind::GcdModNative;
    RecurrenceKind kind = options[rng.uniformInt(0, n - 1)];

    auto root = makeBlock();

    if (kind == RecurrenceKind::GcdSubtract) {
        root->body.push_back(makeAssign(a, makeArrayAccess(makeIntConst(0))));  // a = arr[0]
        root->body.push_back(makeAssign(b, makeArrayAccess(makeIntConst(1))));  // b = arr[1]

        std::vector<StmtPtr> body;
        std::vector<StmtPtr> thenBody;
        thenBody.push_back(makeAssign(a, makeBinOp(ExprKind::Sub, makeVar(a), makeVar(b))));
        std::vector<StmtPtr> elseBody;
        elseBody.push_back(makeAssign(b, makeBinOp(ExprKind::Sub, makeVar(b), makeVar(a))));
        body.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(a), makeVar(b)), std::move(thenBody), std::move(elseBody)));

        loopsUsed++;
        root->body.push_back(makeWhile(makeCompare(ExprKind::Ne, makeVar(a), makeVar(b)), std::move(body)));
        root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(a)));  // arr[0] = a
        return root;
    }

    if (kind == RecurrenceKind::GcdMod) {
        int r = -1;
        for (int cand = 1; cand < cfg.numVars; ++cand) {
            if (cand != a && cand != b) {
                r = cand;
                break;
            }
        }
        if (r < 0) r = a;  // numVars too small to give r its own slot; degenerate but safe

        root->body.push_back(makeAssign(a, makeArrayAccess(makeIntConst(0))));  // a = arr[0]
        root->body.push_back(makeAssign(b, makeArrayAccess(makeIntConst(1))));  // b = arr[1]

        std::vector<StmtPtr> outerBody;
        outerBody.push_back(makeAssign(r, makeVar(a)));  // r = a
        {
            std::vector<StmtPtr> innerBody;
            innerBody.push_back(makeAssign(r, makeBinOp(ExprKind::Sub, makeVar(r), makeVar(b))));  // r = r - b
            loopsUsed++;
            // r ends up as a mod b: repeated subtraction until r < b.
            outerBody.push_back(makeWhile(makeCompare(ExprKind::Ge, makeVar(r), makeVar(b)), std::move(innerBody)));
        }
        outerBody.push_back(makeAssign(a, makeVar(b)));  // a = b
        outerBody.push_back(makeAssign(b, makeVar(r)));  // b = a mod b

        loopsUsed++;
        root->body.push_back(makeWhile(makeCompare(ExprKind::Ne, makeVar(b), makeIntConst(0)), std::move(outerBody)));
        root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(a)));  // arr[0] = a
        return root;
    }

    if (kind == RecurrenceKind::GcdModNative) {
        int r = -1;
        for (int cand = 1; cand < cfg.numVars; ++cand) {
            if (cand != a && cand != b) {
                r = cand;
                break;
            }
        }
        if (r < 0) r = a;  // numVars too small to give r its own slot; degenerate but safe

        root->body.push_back(makeAssign(a, makeArrayAccess(makeIntConst(0))));  // a = arr[0]
        root->body.push_back(makeAssign(b, makeArrayAccess(makeIntConst(1))));  // b = arr[1]

        std::vector<StmtPtr> body;
        body.push_back(makeAssign(r, makeBinOp(ExprKind::Mod, makeVar(a), makeVar(b))));  // r = a % b (O(1))
        body.push_back(makeAssign(a, makeVar(b)));                                        // a = b
        body.push_back(makeAssign(b, makeVar(r)));                                        // b = r

        loopsUsed++;
        root->body.push_back(makeWhile(makeCompare(ExprKind::Ne, makeVar(b), makeIntConst(0)), std::move(body)));
        root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(a)));  // arr[0] = a
        return root;
    }

    // FibonacciAdd
    int i = -1;
    for (int cand = 1; cand < cfg.numVars; ++cand) {
        if (cand != a && cand != b) {
            i = cand;
            break;
        }
    }
    if (i < 0) i = a;  // numVars too small to give i its own slot; degenerate but safe

    root->body.push_back(makeAssign(a, makeIntConst(0)));  // a = 0  (fib(0))
    root->body.push_back(makeAssign(b, makeIntConst(1)));  // b = 1  (fib(1))
    root->body.push_back(makeAssign(i, makeIntConst(0)));  // i = 0

    std::vector<StmtPtr> body;
    body.push_back(makeAssign(b, makeBinOp(ExprKind::Add, makeVar(a), makeVar(b))));  // b = a + b
    body.push_back(makeAssign(a, makeBinOp(ExprKind::Sub, makeVar(b), makeVar(a))));  // a = b - a
    body.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));

    loopsUsed++;
    // arr[0] holds n throughout (never touched until the write-back below),
    // so the loop condition can read it directly without a separate
    // captured copy.
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeArrayAccess(makeIntConst(0))), std::move(body)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(a)));  // arr[0] = a

    return root;
}

// Which nested multiply-via-repeated-addition genRepeatedMultiplySkeletonBlock
// builds. Factorial and Power used to be two separate top-level Skeleton
// options; merged for the same reason as RecurrenceKind above -- they are
// the SAME operation (Mini-C has no multiply primitive, so `p = p * k` is
// built as a nested loop: outer over however many multiplications are
// needed, inner adding `p` to a fresh accumulator `q` exactly `k` times),
// differing only in what `k` is: an INCREASING factor (Factorial: i, 2..n)
// or a CONSTANT captured once (Power: base, captured from arr[0]).
enum class RepeatedMultiplyKind { IncreasingFactor, ConstantFactor };

// IncreasingFactor (factorial): multiplies by i, 2..arr[0], i.e. p
// becomes arr[0]!. Needs 4 scratch scalars (p, i, q, j).
// ConstantFactor (power): multiplies by a captured `base = arr[0]`,
// exactly `arr[1]` times, i.e. p becomes base^exponent. Needs one more
// scratch scalar than IncreasingFactor (the captured base can't reuse the
// outer counter's slot, since the outer counter here just counts
// iterations rather than being the multiplicand itself) -- so
// ConstantFactor is only chosen when numVars leaves room for it; with
// exactly 5 scratch scalars available, this skeleton always builds
// IncreasingFactor (Factorial's original numVars>=5 gate is preserved
// exactly, not narrowed by folding Power in alongside it).
StmtPtr genRepeatedMultiplySkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    bool canConstantFactor = cfg.numVars >= 6;  // needs the extra captured-base scratch scalar
    RepeatedMultiplyKind kind =
        (canConstantFactor && rng.chance(0.5)) ? RepeatedMultiplyKind::ConstantFactor : RepeatedMultiplyKind::IncreasingFactor;

    int slotsNeeded = (kind == RepeatedMultiplyKind::ConstantFactor) ? 5 : 4;
    int vars[5];
    int used = 0;
    while (used < slotsNeeded) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }

    auto root = makeBlock();

    if (kind == RepeatedMultiplyKind::ConstantFactor) {
        int base = vars[0], p = vars[1], i = vars[2], q = vars[3], j = vars[4];
        root->body.push_back(makeAssign(base, makeArrayAccess(makeIntConst(0))));  // base = arr[0]
        root->body.push_back(makeAssign(p, makeIntConst(1)));                      // p = 1  (base^0)
        root->body.push_back(makeAssign(i, makeIntConst(0)));                      // i = 0

        std::vector<StmtPtr> outerBody;
        outerBody.push_back(makeAssign(q, makeIntConst(0)));
        outerBody.push_back(makeAssign(j, makeIntConst(0)));
        {
            std::vector<StmtPtr> innerBody;
            innerBody.push_back(makeAssign(q, makeBinOp(ExprKind::Add, makeVar(q), makeVar(p))));
            innerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));
            loopsUsed++;
            // Inner loop runs exactly `base` times, adding the running
            // result `p` to `q` each time -- turns "+base" into "*base".
            outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(j), makeVar(base)), std::move(innerBody)));
        }
        outerBody.push_back(makeAssign(p, makeVar(q)));  // p = q  (p is now p*base)
        outerBody.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));

        loopsUsed++;
        // arr[1] holds the exponent throughout; the outer loop runs
        // exactly that many times.
        root->body.push_back(
            makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeArrayAccess(makeIntConst(1))), std::move(outerBody)));
        root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(p)));  // arr[0] = p
        return root;
    }

    int p = vars[0], i = vars[1], q = vars[2], j = vars[3];
    root->body.push_back(makeAssign(p, makeIntConst(1)));  // p = 1
    root->body.push_back(makeAssign(i, makeIntConst(2)));  // i = 2

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(q, makeIntConst(0)));  // q = 0
    outerBody.push_back(makeAssign(j, makeIntConst(0)));  // j = 0
    {
        std::vector<StmtPtr> innerBody;
        innerBody.push_back(makeAssign(q, makeBinOp(ExprKind::Add, makeVar(q), makeVar(p))));  // q = q + p
        innerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));
        loopsUsed++;
        outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(j), makeVar(i)), std::move(innerBody)));
    }
    outerBody.push_back(makeAssign(p, makeVar(q)));  // p = q  (p is now p*i)
    outerBody.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));

    loopsUsed++;
    // arr[0] holds n throughout; condition is i < n+1 (i.e. i <= n), i.e.
    // native Le would also work here but this predates it and there's no
    // reason to touch a working comparison just to use the newer operator.
    auto nPlusOne = makeBinOp(ExprKind::Add, makeArrayAccess(makeIntConst(0)), makeIntConst(1));
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), std::move(nPlusOne)), std::move(outerBody)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(p)));  // arr[0] = p

    return root;
}

// Dedicated skeleton for AverageProblem's shape: sum the array (exactly
// the Sum AccumKind's loop, see genCountingLoopBlock), then divide that
// sum by the array length (`arr[0]`... no -- by var 0, the interpreter's
// auto-seeded array-length scalar, see Interpreter::run) via repeated
// subtraction. Composing this from two INDEPENDENTLY drawn generic
// skeletons was considered and rejected: each skeleton call picks its own
// fresh random scratch variables, so a separately-drawn "divide" template
// would need to, by pure chance, pick the exact same variable the Sum
// loop happened to accumulate into as its dividend -- vanishingly
// unlikely, the same composability wall FINDINGS.md section 7 documents
// for min/max/sum's own increment idiom (the causal unit is the whole
// coordinated skeleton, not independently-drawn pieces of one). Offered
// as one atomic two-loop block instead, same precedent as the
// SelectionSort library-learning experiment below. Needs 4 scratch
// scalars (vi, sum, remainder, quotient); gated on numVars >= 5.
StmtPtr genAverageSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[4];
    int used = 0;
    while (used < 4) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int vi = vars[0], sum = vars[1], remainder = vars[2], quotient = vars[3];

    auto root = makeBlock();
    root->body.push_back(makeAssign(sum, makeIntConst(0)));  // sum = 0
    root->body.push_back(makeAssign(vi, makeIntConst(0)));   // vi = 0
    {
        std::vector<StmtPtr> body;
        body.push_back(makeAssign(sum, makeBinOp(ExprKind::Add, makeVar(sum), makeArrayAccess(makeVar(vi)))));  // sum = sum + arr[vi]
        body.push_back(makeAssign(vi, makeBinOp(ExprKind::Add, makeVar(vi), makeIntConst(1))));
        loopsUsed++;
        root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(vi), makeVar(0)), std::move(body)));
    }

    root->body.push_back(makeAssign(remainder, makeVar(sum)));  // remainder = sum
    root->body.push_back(makeAssign(quotient, makeIntConst(0)));  // quotient = 0
    {
        std::vector<StmtPtr> body;
        body.push_back(makeAssign(remainder, makeBinOp(ExprKind::Sub, makeVar(remainder), makeVar(0))));  // remainder -= n
        body.push_back(makeAssign(quotient, makeBinOp(ExprKind::Add, makeVar(quotient), makeIntConst(1))));
        loopsUsed++;
        // Native Ge (see ast/Expr.hpp) makes this a plain, direct
        // division-by-repeated-subtraction loop -- no compound condition
        // needed, unlike GcdEuclid's `!=` before Le/Ge/Ne existed.
        root->body.push_back(makeWhile(makeCompare(ExprKind::Ge, makeVar(remainder), makeVar(0)), std::move(body)));
    }
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(quotient)));  // arr[0] = quotient

    return root;
}

// Library-learning experiment: a full Selection Sort as ONE atomic
// primitive, offered alongside the flat single-loop templates rather than
// hoping the search composes it by chance from individually-discovered
// pieces (adjacent compare-and-swap, a counting loop, ...). This is a
// direct test of the "recurrent block injection" idea -- if the search
// already knows how to find-min-in-a-range and swap-into-place as one
// coordinated unit, does it converge to a genuinely generalizing sort
// noticeably faster/more reliably than composing it from scratch? See the
// A/B note in the commit this landed in for the result.
//   i = 0;
//   while (i < n) {
//     mi = i;
//     j = i + 1;
//     while (j < n) {
//       if (arr[j] < arr[mi]) { mi = j; }
//       j = j + 1;
//     }
//     swap(arr[i], arr[mi]);
//     i = i + 1;
//   }
// Needs 3 scratch scalars (i, mi, j), fitting exactly in the default
// numVars=4 (v0 reserved + 3 free), same tightness as the Membership
// skeleton -- gated the same way (>= 4) even though it happens to also
// need both loop slots like NestedDivisor/Factorial.
StmtPtr genSelectionSortSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[3];
    int used = 0;
    while (used < 3) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int i = vars[0], mi = vars[1], j = vars[2];

    auto root = makeBlock();
    root->body.push_back(makeAssign(i, makeIntConst(0)));  // i = 0

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(mi, makeVar(i)));                                       // mi = i
    outerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));  // j = i + 1
    {
        std::vector<StmtPtr> innerBody;
        {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(mi, makeVar(j)));
            innerBody.push_back(
                makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(makeVar(j)), makeArrayAccess(makeVar(mi))), std::move(then)));
        }
        innerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));
        loopsUsed++;
        outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(j), makeVar(0)), std::move(innerBody)));
    }
    outerBody.push_back(makeSwap(makeVar(i), makeVar(mi)));
    outerBody.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));

    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeVar(0)), std::move(outerBody)));

    return root;
}

// Attempt at O(n log n) sorting -- see FINDINGS.md section 13 for the
// architecture discussion this follows from. Merge sort needs a second
// scratch array (Mini-C has exactly one, doubling as both input and
// output); quicksort needs recursion or an explicit stack (Mini-C has
// neither -- no function calls, no dynamic structures beyond the one
// array). Heapsort is the shape that fits: fully in-place, iterative,
// only needs comparisons/swaps on the one array it already has. Needs
// FOUR while-loops (a build-heap loop wrapping a sift-down loop, then an
// extract-max loop wrapping its own sift-down loop) -- see
// GenConfig::maxLoops, raised from 2 to 4 specifically for this.
//
// Two adaptations for what Mini-C's grammar can and can't express:
//  1. No multiply/divide, so the classic build-heap start index
//     (`n/2 - 1`) is unreachable. Starts from `n-1` instead (every index,
//     not just internal nodes) -- sift-down from a leaf immediately finds
//     no valid child and becomes a no-op, so this is still a CORRECT
//     max-heap construction, just doing some redundant no-op work; it
//     doesn't change the overall O(n log n) bound (build-heap is already
//     dominated by the O(n log n) extract phase either way).
//  2. No `break` statement, so sift-down's usual "loop until no swap
//     needed" is a `while (done == 0) { ...; if (no swap needed) done = 1;
//     }` flag loop instead. No `&&`/`||` either (Mini-C's only multi-
//     condition trick is the additive != from section 9, which doesn't
//     generalize to AND) -- composed via nested `if` instead, which gives
//     the same short-circuit-AND behavior for free through control flow,
//     no boolean-expression algebra needed.
//
// Needs 4 scratch scalars, reused across both phases since they run
// strictly sequentially (build-heap fully finishes before extraction
// starts, so there's no lifetime conflict): `k` (the outer loop's
// counter, first counting down from n-1 for build-heap, then reused
// counting down from n-1 for extraction), `root`/`child` (sift-down's
// current node and the child it's comparing against), `done` (the
// sift-down flag). Gated on numVars >= 5 and a full 4-loop budget.
StmtPtr genHeapSortSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[4];
    int used = 0;
    while (used < 4) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int k = vars[0], root = vars[1], child = vars[2], done = vars[3];

    // Builds one sift-down: `root = <rootInit>; done = 0; while (done==0)
    // { child = root+root+1; if (child >= size) done=1; else { if
    // (child+1 < size) { if (arr[child+1] > arr[child]) child=child+1; }
    // if (arr[root] >= arr[child]) done=1; else { swap(arr[root],
    // arr[child]); root=child; } } }`. `size` is cloned fresh at each of
    // its two uses (child-in-range check, child+1-in-range check).
    auto buildSiftDown = [&](ExprPtr rootInit, ExprPtr sizeExpr, std::vector<StmtPtr>& out) {
        out.push_back(makeAssign(root, std::move(rootInit)));
        out.push_back(makeAssign(done, makeIntConst(0)));

        std::vector<StmtPtr> loopBody;
        loopBody.push_back(makeAssign(child, makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeVar(root), makeVar(root)),
                                                          makeIntConst(1))));  // child = root + root + 1

        std::vector<StmtPtr> thenNoChild;
        thenNoChild.push_back(makeAssign(done, makeIntConst(1)));

        std::vector<StmtPtr> elseHasChild;
        {
            // if (child+1 < size) { if (arr[child+1] > arr[child]) child = child+1; }
            std::vector<StmtPtr> pickRight;
            {
                std::vector<StmtPtr> doPick;
                doPick.push_back(makeAssign(child, makeBinOp(ExprKind::Add, makeVar(child), makeIntConst(1))));
                auto rightIdx = makeBinOp(ExprKind::Add, makeVar(child), makeIntConst(1));
                pickRight.push_back(
                    makeIf(makeCompare(ExprKind::Gt, makeArrayAccess(std::move(rightIdx)), makeArrayAccess(makeVar(child))),
                           std::move(doPick)));
            }
            auto childPlusOne = makeBinOp(ExprKind::Add, makeVar(child), makeIntConst(1));
            elseHasChild.push_back(
                makeIf(makeCompare(ExprKind::Lt, std::move(childPlusOne), sizeExpr->clone()), std::move(pickRight)));

            // if (arr[root] >= arr[child]) done = 1; else { swap; root = child; }
            std::vector<StmtPtr> stop;
            stop.push_back(makeAssign(done, makeIntConst(1)));
            std::vector<StmtPtr> descend;
            descend.push_back(makeSwap(makeVar(root), makeVar(child)));
            descend.push_back(makeAssign(root, makeVar(child)));
            elseHasChild.push_back(makeIf(makeCompare(ExprKind::Ge, makeArrayAccess(makeVar(root)), makeArrayAccess(makeVar(child))),
                                            std::move(stop), std::move(descend)));
        }
        loopBody.push_back(
            makeIf(makeCompare(ExprKind::Ge, makeVar(child), sizeExpr->clone()), std::move(thenNoChild), std::move(elseHasChild)));

        loopsUsed++;
        out.push_back(makeWhile(makeCompare(ExprKind::Eq, makeVar(done), makeIntConst(0)), std::move(loopBody)));
    };

    auto rootAst = makeBlock();

    // Phase 1: build max-heap. k = n-1 downto 0, sift-down(k) with heap
    // size fixed at n (var 0).
    rootAst->body.push_back(makeAssign(k, makeBinOp(ExprKind::Sub, makeVar(0), makeIntConst(1))));  // k = n - 1
    {
        std::vector<StmtPtr> buildBody;
        buildSiftDown(makeVar(k), makeVar(0), buildBody);
        buildBody.push_back(makeAssign(k, makeBinOp(ExprKind::Sub, makeVar(k), makeIntConst(1))));
        loopsUsed++;
        rootAst->body.push_back(makeWhile(makeCompare(ExprKind::Ge, makeVar(k), makeIntConst(0)), std::move(buildBody)));
    }

    // Phase 2: extract max repeatedly. k = n-1 downto 1: swap arr[0] with
    // arr[k] (the current largest to its final sorted slot), then
    // sift-down(0) with heap size shrunk to k.
    rootAst->body.push_back(makeAssign(k, makeBinOp(ExprKind::Sub, makeVar(0), makeIntConst(1))));  // k = n - 1 (reused)
    {
        std::vector<StmtPtr> extractBody;
        extractBody.push_back(makeSwap(makeIntConst(0), makeVar(k)));
        buildSiftDown(makeIntConst(0), makeVar(k), extractBody);
        extractBody.push_back(makeAssign(k, makeBinOp(ExprKind::Sub, makeVar(k), makeIntConst(1))));
        loopsUsed++;
        rootAst->body.push_back(makeWhile(makeCompare(ExprKind::Ge, makeVar(k), makeIntConst(1)), std::move(extractBody)));
    }

    return rootAst;
}

// Dedicated skeleton for 3SUM-shaped problems: does any triple of DISTINCT
// indices i<j<k sum to zero? See FINDINGS.md section 15 -- added after the
// free search couldn't find a genuinely generalizing solution even with a
// 10x budget bump, same "give the search the shape it needs" motivation as
// every other dedicated skeleton here (SelectionSort, NestedDivisor, ...).
//
//   i = 0;
//   found = 0;
//   while (i < v0) {
//     j = i + 1;
//     while (j < v0) {
//       k = j + 1;
//       while (k < v0) {
//         if ((arr[i] + arr[j]) + arr[k] == 0) { found = 1; }
//         k = k + 1;
//       }
//       j = j + 1;
//     }
//     i = i + 1;
//   }
//   arr[0] = found;
//
// A genuine triple-nested loop, not a template that happens to reuse two
// loops like NestedDivisor/RepeatedMultiply do -- needs a full 3-loop
// budget (gated on canTripleLoop, i.e. maxLoops - loopsUsed >= 3, same
// budget arithmetic as canQuadLoop one tier down) and 4 scratch scalars
// (i, j, k, found), one more than SelectionSort's 3 since the found flag
// can't double as a loop index the way SelectionSort's `mi` does double
// duty. Gated on numVars >= 5, same tier as NestedDivisor/Average.
StmtPtr genThreeSumSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[4];
    int used = 0;
    while (used < 4) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int i = vars[0], j = vars[1], k = vars[2], found = vars[3];

    auto root = makeBlock();
    root->body.push_back(makeAssign(i, makeIntConst(0)));      // i = 0
    root->body.push_back(makeAssign(found, makeIntConst(0)));  // found = 0

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));  // j = i + 1
    {
        std::vector<StmtPtr> midBody;
        midBody.push_back(makeAssign(k, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));  // k = j + 1
        {
            std::vector<StmtPtr> innerBody;
            {
                auto sum = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeArrayAccess(makeVar(i)), makeArrayAccess(makeVar(j))),
                                      makeArrayAccess(makeVar(k)));
                std::vector<StmtPtr> then;
                then.push_back(makeAssign(found, makeIntConst(1)));
                innerBody.push_back(makeIf(makeCompare(ExprKind::Eq, std::move(sum), makeIntConst(0)), std::move(then)));
            }
            innerBody.push_back(makeAssign(k, makeBinOp(ExprKind::Add, makeVar(k), makeIntConst(1))));
            loopsUsed++;
            midBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(k), makeVar(0)), std::move(innerBody)));
        }
        midBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));
        loopsUsed++;
        outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(j), makeVar(0)), std::move(midBody)));
    }
    outerBody.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));
    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeVar(0)), std::move(outerBody)));

    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(found)));  // arr[0] = found

    return root;
}

// Dedicated skeleton for GraphReachabilityProblem (FINDINGS.md section 16):
// is vertex (V-1) reachable from vertex 0 in a directed graph, stored as
// [V, V scratch "reached" cells, V*V row-major adjacency matrix] (see the
// Problem's own header comment for the exact layout)? Fixed-point
// relaxation, not real BFS/DFS (Mini-C has no queue/stack): repeat V times,
// each round scanning every (i,j) pair and marking j reached whenever i is
// already reached and an edge i->j exists. V rounds is always enough since
// the longest possible simple path has fewer than V edges.
//
//   nv = arr[0];
//   arr[1] = 1;                    // reached[0] = 1
//   base = nv + 1;                 // where the adjacency matrix starts
//   round = 0;
//   while (round < nv) {
//     i = 0;
//     while (i < nv) {
//       if (arr[1+i] == 1) {
//         j = 0;
//         while (j < nv) {
//           if (arr[base + (i*nv+j)] == 1) { arr[1+j] = 1; }
//           j = j + 1;
//         }
//       }
//       i = i + 1;
//     }
//     round = round + 1;
//   }
//   arr[0] = arr[nv];              // reached[V-1] lives at arr[1+(nv-1)] = arr[nv]
//
// Genuinely needs the native Mul primitive for the i*nv+j row-major index --
// unreachable before FINDINGS.md section 16's arithmetic extension. A true
// triple-nested loop (round, i, j), so gated on canTripleLoop like ThreeSum;
// needs 5 scratch scalars (nv, base, round, i, j), so numVars >= 6, one
// tier up from ThreeSum's 5.
StmtPtr genGraphReachabilitySkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int vars[5];
    int used = 0;
    while (used < 5) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int k = 0; k < used; ++k) {
            if (vars[k] == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars[used++] = cand;
    }
    int nv = vars[0], base = vars[1], round = vars[2], i = vars[3], j = vars[4];

    auto root = makeBlock();
    root->body.push_back(makeAssign(nv, makeArrayAccess(makeIntConst(0))));                       // nv = arr[0]
    root->body.push_back(makeAssignArray(makeIntConst(1), makeIntConst(1)));                      // arr[1] = 1
    root->body.push_back(makeAssign(base, makeBinOp(ExprKind::Add, makeVar(nv), makeIntConst(1))));  // base = nv + 1
    root->body.push_back(makeAssign(round, makeIntConst(0)));

    std::vector<StmtPtr> roundBody;
    roundBody.push_back(makeAssign(i, makeIntConst(0)));
    {
        std::vector<StmtPtr> iBody;
        {
            std::vector<StmtPtr> ifReachedI;
            ifReachedI.push_back(makeAssign(j, makeIntConst(0)));
            {
                std::vector<StmtPtr> jBody;
                {
                    auto edgeIdx = makeBinOp(ExprKind::Add, makeVar(base),
                                              makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Mul, makeVar(i), makeVar(nv)), makeVar(j)));
                    std::vector<StmtPtr> markReached;
                    markReached.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(j)), makeIntConst(1)));
                    jBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeArrayAccess(std::move(edgeIdx)), makeIntConst(1)),
                                            std::move(markReached)));
                }
                jBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));
                loopsUsed++;
                ifReachedI.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(j), makeVar(nv)), std::move(jBody)));
            }
            iBody.push_back(makeIf(
                makeCompare(ExprKind::Eq, makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i))), makeIntConst(1)),
                std::move(ifReachedI)));
        }
        iBody.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));
        loopsUsed++;
        roundBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeVar(nv)), std::move(iBody)));
    }
    roundBody.push_back(makeAssign(round, makeBinOp(ExprKind::Add, makeVar(round), makeIntConst(1))));
    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(round), makeVar(nv)), std::move(roundBody)));

    root->body.push_back(makeAssignArray(makeIntConst(0), makeArrayAccess(makeVar(nv))));  // arr[0] = arr[nv] (reached[V-1])

    return root;
}

// A self-increment `v = v + 1` for a random variable -- the specific
// idiom the forensic analysis found in 94.5% of generalizing programs
// (100% for min/max/sum) vs. 37.7% of non-generalizers.
StmtPtr genIndexIncrementStmt(Random& rng, const GenConfig& cfg) {
    int v = rng.uniformInt(0, cfg.numVars - 1);
    return makeAssign(v, makeBinOp(ExprKind::Add, makeVar(v), makeIntConst(1)));
}

StmtPtr genStmt(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, bool insideIf = false,
                bool insideWhile = false) {
    // Weighted choice among available statement kinds.
    bool canNest = depthBudget > 0;
    bool canLoop = canNest && loopsUsed < cfg.maxLoops;
    bool canNestedLoop = canNest && (cfg.maxLoops - loopsUsed) >= 2;
    bool canTripleLoop = canNest && (cfg.maxLoops - loopsUsed) >= 3;  // ThreeSum's 3-loop shape
    bool canQuadLoop = canNest && (cfg.maxLoops - loopsUsed) >= 4;    // heapsort's 4-loop shape

    // One universal trigger probability, not one flag per problem: a tool
    // that hands the search a different CLI knob per problem it's meant to
    // discover algorithms FOR isn't a general synthesizer anymore, it's a
    // human already knowing the answer and telling the generator which
    // template to use. So there is exactly one bias here
    // (loopAccumulatorBias, the same one already tuned at 0.4) that
    // triggers identically for every problem; what differs is only which
    // template gets PICKED once triggered, uniformly at random among
    // whichever ones fit the current loop budget. The search's own scoring
    // is what decides whether a given template ends up being useful for
    // whatever problem is actually running -- nothing here is told in
    // advance that "gcd needs the Euclid shape".
    //
    // Six top-level shapes, not eight: Recurrence folds GcdEuclid+
    // Fibonacci into one draw (both "seed two scalars, update via Add/Sub
    // each loop iteration"), and RepeatedMultiply folds Factorial+Power
    // into one draw (both "multiply via a nested repeated-addition loop"
    // -- differing only in whether the multiplicand increases or is
    // constant). Each keeps choosing its own internal variant (see
    // RecurrenceKind/RepeatedMultiplyKind) -- this is about the SAMPLING
    // surface (loopAccumulatorBias's one trigger spreads its probability
    // across fewer, more general slots), not about pretending unrelated
    // algorithms are the same computation. NestedDivisor, SelectionSort,
    // and Average stay separate: none of them share a loop shape with
    // anything else here closely enough that merging wouldn't just be an
    // artificial wrapper around unrelated bodies (arithmetic recurrence
    // vs. array-element comparison-and-swap vs. two sequential loops).
    if (canLoop && cfg.numVars >= 3 && cfg.loopAccumulatorBias > 0.0 && rng.chance(cfg.loopAccumulatorBias)) {
        enum class Skeleton { Counting, Recurrence, NestedDivisor, NestedDivisorNative, SelectionSort, RepeatedMultiply, Average, HeapSort, ThreeSum, GraphReachability };
        // Research-only override, never a production knob (matches the
        // ALGOSEARCH_DEBUG_LANDSCAPE/ALGOSEARCH_DEBUG_ISLANDS diagnostic
        // env-var pattern elsewhere in this codebase): restricts every
        // skeleton draw to exactly one named shape, for a cross-skeleton
        // study of whether a problem's search ever converges using a
        // skeleton that wasn't built for it. Deliberately an env var, not
        // a CLI flag -- the whole reason per-problem skeleton bias flags
        // were removed (see GenConfig::loopAccumulatorBias's comment) was
        // to keep the CLI itself from revealing which algorithm a given
        // problem needs; an env var used for a one-off cross-skeleton
        // experiment doesn't reintroduce that, since it's the same
        // override for every problem in the run, not a per-problem
        // pre-known answer. Unset (the default) reproduces the uniform
        // random choice among whichever shapes fit the loop/numVars
        // budget, exactly as before this existed. An unrecognized or
        // budget-incompatible name yields zero options, which just skips
        // the skeleton branch entirely for that statement (falls through
        // to the plain unstructured grammar below) rather than erroring.
        const char* forcedSkeletonEnv = std::getenv("ALGOSEARCH_FORCE_SKELETON");
        auto allowed = [&](const char* name) {
            return forcedSkeletonEnv == nullptr || std::strcmp(forcedSkeletonEnv, name) == 0;
        };

        // Weighted, not uniform: HeapSort is a much bigger AST (~131
        // nodes for a small array) than any other skeleton here (all
        // under 30), and giving it an equal 1-in-7 draw measurably slowed
        // down EVERY problem's search, not just sorting's -- confirmed
        // directly: `reverse` at a modest budget went from ~15s to not
        // converging in 3+ minutes once HeapSort joined the uniform
        // draw, because a large, structurally-irrelevant-for-reverse
        // candidate eating a 1-in-7 share of every biased statement
        // pollutes the candidate pool at every size step for every
        // problem, not just the ones it could help. Weight it at 1/3 of
        // a normal skeleton's share instead -- still reachable when it's
        // actually useful (sorting), rare enough elsewhere to stop
        // costing everyone else. Weight is a total-weight-then-roll
        // scheme (options carry a weight, not literal repeated entries)
        // to avoid quietly changing the RNG's per-call consumption
        // pattern for other skeletons based on how many array slots they
        // occupy.
        struct WeightedOption {
            Skeleton skeleton;
            int weight;
        };
        WeightedOption options[10];
        int n = 0;
        int totalWeight = 0;
        auto addOption = [&](Skeleton s, int w) {
            options[n++] = {s, w};
            totalWeight += w;
        };
        if (allowed("counting")) addOption(Skeleton::Counting, 3);
        if (allowed("recurrence")) addOption(Skeleton::Recurrence, 3);
        if (canNestedLoop && cfg.numVars >= 5 && allowed("nesteddivisor")) addOption(Skeleton::NestedDivisor, 3);
        if (cfg.numVars >= 3 && allowed("nesteddivisornative")) addOption(Skeleton::NestedDivisorNative, 3);
        if (canNestedLoop && cfg.numVars >= 4 && allowed("selectionsort")) addOption(Skeleton::SelectionSort, 3);
        if (canNestedLoop && cfg.numVars >= 5 && allowed("repeatedmultiply")) addOption(Skeleton::RepeatedMultiply, 3);
        if (canNestedLoop && cfg.numVars >= 5 && allowed("average")) addOption(Skeleton::Average, 3);
        if (canQuadLoop && cfg.numVars >= 5 && allowed("heapsort")) addOption(Skeleton::HeapSort, 1);
        if (canTripleLoop && cfg.numVars >= 5 && allowed("threesum")) addOption(Skeleton::ThreeSum, 3);
        if (canTripleLoop && cfg.numVars >= 6 && allowed("graphreachability")) addOption(Skeleton::GraphReachability, 3);
        if (n > 0) {
            int roll = rng.uniformInt(0, totalWeight - 1);
            Skeleton chosen = options[0].skeleton;
            for (int i = 0; i < n; ++i) {
                if (roll < options[i].weight) {
                    chosen = options[i].skeleton;
                    break;
                }
                roll -= options[i].weight;
            }
            switch (chosen) {
                case Skeleton::Counting:
                    return genCountingLoopBlock(rng, cfg, loopsUsed);
                case Skeleton::Recurrence:
                    return genRecurrenceSkeletonBlock(rng, cfg, loopsUsed, canNestedLoop);
                case Skeleton::NestedDivisor:
                    return genNestedDivisorLoopBlock(rng, cfg, loopsUsed);
                case Skeleton::NestedDivisorNative:
                    return genNestedDivisorNativeLoopBlock(rng, cfg, loopsUsed);
                case Skeleton::SelectionSort:
                    return genSelectionSortSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::HeapSort:
                    return genHeapSortSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::RepeatedMultiply:
                    return genRepeatedMultiplySkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::Average:
                    return genAverageSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::ThreeSum:
                    return genThreeSumSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::GraphReachability:
                    return genGraphReachabilitySkeletonBlock(rng, cfg, loopsUsed);
            }
        }
    }

    int maxChoice = canNest ? (canLoop ? 4 : 3) : 1;
    int choice = rng.uniformInt(0, maxChoice);

    switch (choice) {
        case 0:
            if (insideWhile && cfg.indexIncrementBias > 0.0 && rng.chance(cfg.indexIncrementBias)) {
                return genIndexIncrementStmt(rng, cfg);
            }
            return makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
        case 1: {
            double swapChance = insideIf ? 0.5 : std::max(0.05, 0.5 - cfg.bareSwapPenalty);
            if (!rng.chance(swapChance)) {
                return makeAssignArray(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
            }
            return makeSwap(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
        }
        case 2: {
            auto thenBody = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/true,
                                           /*insideWhile=*/false);
            std::vector<StmtPtr> elseBody;
            if (rng.chance(0.4))
                elseBody = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/true,
                                          /*insideWhile=*/false);
            return makeIf(genCondExpr(rng, cfg.maxExprDepth, cfg), std::move(thenBody), std::move(elseBody));
        }
        case 3:
            if (insideWhile && cfg.indexIncrementBias > 0.0 && rng.chance(cfg.indexIncrementBias)) {
                return genIndexIncrementStmt(rng, cfg);
            }
            return makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
        case 4: {
            loopsUsed++;
            auto body = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/false,
                                       /*insideWhile=*/true);
            return makeWhile(genCondExpr(rng, cfg.maxExprDepth, cfg), std::move(body));
        }
        default:
            return makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
    }
}

std::vector<StmtPtr> genBlockStmts(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, int maxStmts,
                                    bool insideIf, bool insideWhile) {
    std::vector<StmtPtr> stmts;
    int n = rng.uniformInt(1, std::max(1, maxStmts));
    for (int i = 0; i < n; ++i) {
        stmts.push_back(genStmt(rng, depthBudget, cfg, loopsUsed, insideIf, insideWhile));
    }
    return stmts;
}

int countLoops(const Stmt& s) {
    int total = (s.kind == StmtKind::While) ? 1 : 0;
    for (const auto& c : s.body) total += countLoops(*c);
    for (const auto& c : s.elseBody) total += countLoops(*c);
    return total;
}

// A growProgram() insertion target: the block itself, plus what kind of
// statement it's the direct body of, so an inserted statement can be
// generated with the right insideIf/insideWhile context (see genStmt) --
// without this, growProgram would silently bypass the structural priors
// for every statement it inserts, since context can't be inferred later.
struct BlockTarget {
    std::vector<StmtPtr>* block;
    StmtKind parentKind;
};

// Collects mutable references to every block-like statement vector in the
// tree (the root's body, plus every if/while body and if's else-body).
void collectBlocks(Stmt& s, std::vector<BlockTarget>& out) {
    // Only Block/If/While actually execute and print their `body`; only If
    // uses `elseBody`. Registering any other vector as an insertion target
    // would make inserted statements invisible to both execution and print.
    if (s.kind == StmtKind::Block || s.kind == StmtKind::If || s.kind == StmtKind::While) {
        out.push_back({&s.body, s.kind});
    }
    if (s.kind == StmtKind::If) {
        out.push_back({&s.elseBody, s.kind});
    }
    for (auto& c : s.body) collectBlocks(*c, out);
    for (auto& c : s.elseBody) collectBlocks(*c, out);
}

// Collects every statement node in the tree (read-only), for picking an
// arbitrary donor subtree in crossover -- deliberately every node, not
// just top-level block members, so a deeply nested `if` or `while` found
// in one lineage can be transplanted whole into another.
void collectAllStmts(const Stmt& s, std::vector<const Stmt*>& out) {
    out.push_back(&s);
    for (const auto& c : s.body) collectAllStmts(*c, out);
    for (const auto& c : s.elseBody) collectAllStmts(*c, out);
}

}  // namespace

Program generateRandomProgram(Random& rng, int targetSize, const GenConfig& cfg) {
    auto root = makeBlock();
    int loopsUsed = 0;
    int guard = 0;
    while (root->size() < targetSize && guard < 200) {
        guard++;
        root->body.push_back(genStmt(rng, cfg.maxBlockDepth, cfg, loopsUsed));
        if (static_cast<int>(root->body.size()) > cfg.maxStatementsPerBlock * 4) break;
    }
    return root;
}

Program growProgram(const Stmt& seed, int targetSize, Random& rng, const GenConfig& cfg) {
    auto root = seed.clone();
    int loopsUsed = countLoops(*root);
    int guard = 0;
    while (root->size() < targetSize && guard < 200) {
        guard++;
        std::vector<BlockTarget> blocks;
        collectBlocks(*root, blocks);
        if (blocks.empty()) break;
        const BlockTarget& targetInfo = blocks[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(blocks.size()) - 1))];
        auto* target = targetInfo.block;
        size_t pos = target->empty() ? 0 : static_cast<size_t>(rng.uniformInt(0, static_cast<int>(target->size())));
        target->insert(target->begin() + static_cast<long>(pos),
                        genStmt(rng, cfg.maxBlockDepth, cfg, loopsUsed, /*insideIf=*/targetInfo.parentKind == StmtKind::If,
                                /*insideWhile=*/targetInfo.parentKind == StmtKind::While));
    }

    // Point-mutate: replace one existing statement in place. This is what
    // lets beam search refine logic (not just append more of it) -- pure
    // insertion can only ever add code, never correct a wrong comparison or
    // wrong index deep inside an already-large candidate.
    if (rng.chance(0.6)) {
        std::vector<BlockTarget> blocks;
        collectBlocks(*root, blocks);
        std::vector<BlockTarget> nonEmpty;
        for (const auto& b : blocks) {
            if (!b.block->empty()) nonEmpty.push_back(b);
        }
        if (!nonEmpty.empty()) {
            const BlockTarget& targetInfo = nonEmpty[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(nonEmpty.size()) - 1))];
            auto* target = targetInfo.block;
            size_t idx = static_cast<size_t>(rng.uniformInt(0, static_cast<int>(target->size()) - 1));
            int loopsUsed = countLoops(*root);
            (*target)[idx] = genStmt(rng, cfg.maxBlockDepth, cfg, loopsUsed, /*insideIf=*/targetInfo.parentKind == StmtKind::If,
                                      /*insideWhile=*/targetInfo.parentKind == StmtKind::While);
        }
    }
    return root;
}

// Subtree crossover: the ONE mutation operator missing from this search
// entirely before this function existed. growProgram only ever inserts or
// replaces a statement with a BRAND NEW randomly generated one -- every
// beam lineage evolves in total isolation, so if the correct algorithm
// needs two or three independent structural insights (a loop bound, a
// comparison shape, a body update) to all land in the SAME candidate at
// once, mutation-only search has to stumble into every one of them
// simultaneously in a single random walk. Real genetic programming relies
// on crossover between two independently-fit parents specifically to
// avoid that: if lineage A discovered the right loop shape and lineage B
// (unrelated) discovered the right comparison, crossover can combine them
// in one step instead of requiring either lineage to re-discover the
// other's insight from scratch.
//
// Clones `recipient`, picks a random insertion point in it (reusing the
// same block-target machinery growProgram uses), and splices in a clone
// of a randomly chosen subtree from `donor` -- any statement node at any
// depth, not just top-level ones, so a nested `if`/`while` found in one
// parent can transplant whole into the other.
Program crossoverPrograms(const Stmt& recipient, const Stmt& donor, Random& rng, const GenConfig& cfg) {
    auto root = recipient.clone();

    std::vector<const Stmt*> donorNodes;
    collectAllStmts(donor, donorNodes);
    const Stmt* donorNode = donorNodes[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(donorNodes.size()) - 1))];
    StmtPtr transplant = donorNode->clone();

    std::vector<BlockTarget> blocks;
    collectBlocks(*root, blocks);
    if (blocks.empty()) return root;
    const BlockTarget& targetInfo = blocks[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(blocks.size()) - 1))];
    auto* target = targetInfo.block;

    if (!target->empty() && rng.chance(0.5)) {
        // Replace an existing statement (refines logic in place, same
        // rationale as growProgram's point-mutation pass).
        size_t idx = static_cast<size_t>(rng.uniformInt(0, static_cast<int>(target->size()) - 1));
        (*target)[idx] = std::move(transplant);
    } else {
        // Insert alongside.
        size_t pos = target->empty() ? 0 : static_cast<size_t>(rng.uniformInt(0, static_cast<int>(target->size())));
        target->insert(target->begin() + static_cast<long>(pos), std::move(transplant));
    }
    (void)cfg;  // reserved for future context-aware splicing (e.g. remapping variable indices)
    return root;
}

}  // namespace algosearch
