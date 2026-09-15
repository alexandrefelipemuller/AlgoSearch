#include "search/ProgramGenerator.hpp"

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
    int choice = rng.uniformInt(0, 3);
    switch (choice) {
        case 0:
            return makeArrayAccess(genValueExpr(rng, depthBudget - 1, cfg));
        case 1:
            return makeBinOp(ExprKind::Add, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
        case 2:
            return makeBinOp(ExprKind::Sub, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
        default:
            return makeArrayAccess(genValueExpr(rng, depthBudget - 1, cfg));
    }
}

// Grammar completeness fix: Mini-C has no native "!=" operator, and
// genValueExpr (which builds Add/Sub operands) never nests a comparison
// inside itself, so a compound condition was previously the *only* way
// this generator could ever produce inequality -- and it never did,
// because this function only ever emitted a single Lt/Gt/Eq. That means
// "while (a != b)" was structurally unreachable at any bias or budget, not
// merely rare -- confirmed the hard blocker behind GcdProblem's search
// getting stuck flat on a lucky size-4 program for a full 10-minute run at
// generous beam-width/candidates (subtractive Euclidean GCD needs exactly
// this condition). Comparisons are ints (0/1) and mutually exclusive, so
// (x<y)+(x>y) is 1 iff x != y and 0 iff x == y -- a valid "not equal" built
// entirely from the existing primitives. Included as one of the four
// standard condition shapes (25% each) rather than an opt-in bias knob,
// since it's closing a representational gap, not a search preference --
// pure superset of what the grammar could already express.
ExprPtr genCondExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    if (rng.uniformInt(0, 3) == 3) {
        auto x = genValueExpr(rng, depthBudget, cfg);
        auto y = genValueExpr(rng, depthBudget, cfg);
        auto lt = makeCompare(ExprKind::Lt, x->clone(), y->clone());
        auto gt = makeCompare(ExprKind::Gt, std::move(x), std::move(y));
        return makeBinOp(ExprKind::Add, std::move(lt), std::move(gt));
    }
    ExprKind kinds[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq};
    ExprKind k = kinds[rng.uniformInt(0, 2)];
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
enum class AccumKind { Sum, Min, Max, Membership, Swap };

bool accumKindNeedsWriteBack(AccumKind k) {
    return k == AccumKind::Sum || k == AccumKind::Min || k == AccumKind::Max || k == AccumKind::Membership;
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
    bool canMembership = cfg.numVars >= 4;
    int draw = rng.uniformInt(0, canMembership ? 4 : 3);
    AccumKind kind = static_cast<AccumKind>(canMembership || draw < 3 ? draw : 4);  // skip Membership(3) -> Swap(4)
    bool needsArr0Init = (kind == AccumKind::Min || kind == AccumKind::Max);

    int targetVar = -1;
    auto root = makeBlock();
    if (kind == AccumKind::Membership) {
        for (int cand = 1; cand < cfg.numVars; ++cand) {
            if (cand != vi && cand != acc) {
                targetVar = cand;
                break;
            }
        }
        root->body.push_back(makeAssign(targetVar, makeArrayAccess(makeIntConst(0))));  // target = arr[0]
    }
    root->body.push_back(makeAssign(acc, needsArr0Init ? makeArrayAccess(makeIntConst(0)) : makeIntConst(0)));
    root->body.push_back(makeAssign(vi, makeIntConst(kind == AccumKind::Membership ? 1 : 0)));

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
// for the full rationale. Reserves var 0 (array length, unused
// here) and picks three distinct scalars for x (the value under test), d
// (candidate divisor), and r (repeated-subtraction remainder).
StmtPtr genNestedDivisorLoopBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int x = 1 + rng.uniformInt(0, cfg.numVars - 2);
    int d = 1 + rng.uniformInt(0, cfg.numVars - 2);
    if (d == x) d = 1 + (x % (cfg.numVars - 1));
    int r = -1;
    for (int cand = 1; cand < cfg.numVars; ++cand) {
        if (cand != x && cand != d) {
            r = cand;
            break;
        }
    }
    if (r < 0) r = x;  // numVars too small to give r its own slot; reuse x's (degenerate but safe)

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
    outerBody.push_back(makeAssign(d, makeBinOp(ExprKind::Add, makeVar(d), makeIntConst(1))));  // d = d + 1
    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(d), makeVar(x)), std::move(outerBody)));

    return root;
}

// Dedicated skeleton for the subtractive Euclidean GCD algorithm -- see
// the loopAccumulatorBias comment in ProgramGenerator.hpp for the full
// rationale and shape.
// Reserves var 0 (array length, unused for this 2-scalar-input problem)
// and picks two distinct scalars for a, b.
StmtPtr genGcdEuclidSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int a = 1 + rng.uniformInt(0, cfg.numVars - 2);
    int b = 1 + rng.uniformInt(0, cfg.numVars - 2);
    if (b == a) b = 1 + (a % (cfg.numVars - 1));

    auto root = makeBlock();
    root->body.push_back(makeAssign(a, makeArrayAccess(makeIntConst(0))));  // a = arr[0]
    root->body.push_back(makeAssign(b, makeArrayAccess(makeIntConst(1))));  // b = arr[1]

    std::vector<StmtPtr> body;
    {
        std::vector<StmtPtr> thenBody;
        thenBody.push_back(makeAssign(a, makeBinOp(ExprKind::Sub, makeVar(a), makeVar(b))));
        std::vector<StmtPtr> elseBody;
        elseBody.push_back(makeAssign(b, makeBinOp(ExprKind::Sub, makeVar(b), makeVar(a))));
        body.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(a), makeVar(b)), std::move(thenBody), std::move(elseBody)));
    }
    auto lt = makeCompare(ExprKind::Lt, makeVar(a), makeVar(b));
    auto gt = makeCompare(ExprKind::Gt, makeVar(a), makeVar(b));
    auto cond = makeBinOp(ExprKind::Add, std::move(lt), std::move(gt));  // a != b

    loopsUsed++;
    root->body.push_back(makeWhile(std::move(cond), std::move(body)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(a)));  // arr[0] = a

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
    if (canLoop && cfg.numVars >= 3 && cfg.loopAccumulatorBias > 0.0 && rng.chance(cfg.loopAccumulatorBias)) {
        int numOptions = canNestedLoop ? 3 : 2;
        switch (rng.uniformInt(0, numOptions - 1)) {
            case 0:
                return genCountingLoopBlock(rng, cfg, loopsUsed);
            case 1:
                return genGcdEuclidSkeletonBlock(rng, cfg, loopsUsed);
            default:
                return genNestedDivisorLoopBlock(rng, cfg, loopsUsed);
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

}  // namespace algosearch
