#include "search/ProgramGenerator.hpp"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <set>
#include <vector>

#include "search/HoleGrammar.hpp"

namespace algosearch {

namespace {

// Temporary, research-only instrumentation: counts how often the skeleton
// draw (loopAccumulatorBias's rng.chance trigger) fires at all, and how
// often it specifically lands on BinarySearch, printed at process exit when
// ALGOSEARCH_DEBUG_SKELETON_COUNTS is set. Same non-production status as
// ALGOSEARCH_DEBUG_LANDSCAPE/ALGOSEARCH_DEBUG_ISLANDS elsewhere in this
// file. Generation runs across worker threads, hence atomics.
std::atomic<long> gSkeletonDrawAttempts{0};
std::atomic<long> gSkeletonBinarySearchPicks{0};
struct SkeletonDebugPrinter {
    ~SkeletonDebugPrinter() {
        if (std::getenv("ALGOSEARCH_DEBUG_SKELETON_COUNTS") != nullptr) {
            std::cerr << "[skeleton-debug] draw attempts=" << gSkeletonDrawAttempts.load()
                      << " binarysearch picks=" << gSkeletonBinarySearchPicks.load() << "\n";
        }
    }
};
SkeletonDebugPrinter gSkeletonDebugPrinter;

// Section 66: replace every site of a skeleton block whose expression lies
// in the hole grammar (search/HoleGrammar.hpp) by a random grammar
// expression over the block's own scalars; out-of-grammar sites are kept.
void scrambleHoleSites(Stmt& block, Random& rng) {
    std::set<int> varSet = {0};
    collectVars(block, varSet);
    std::vector<int> vars(varSet.begin(), varSet.end());
    forEachSlot(block, [&](Stmt&, ExprPtr& slot, bool isCond) {
        if (isCond ? inCondGrammar(*slot) : inValueGrammar(*slot)) slot = isCond ? randomCond(rng, vars) : randomValue(rng, vars);
    });
}

// -- Generation-time pruning (GenConfig::pruneGeneration) and its counters --
//
// Phase 0 of FINDINGS.md section 33: every detector below runs whether or
// not pruning is on (none of them touch the RNG, so pruneGeneration=false
// stays byte-identical to before), and the counts are printed at exit when
// ALGOSEARCH_DEBUG_PRUNE_COUNTS is set -- to measure how much of the
// generation budget goes to text-level trivia before deciding whether any
// given rule is worth an A/B at all.
//
// Two groups. PRUNED rules are redrawn when pruneGeneration is on. INFO-only
// patterns are algebraic-identity shapes (`x - x`, `x + 0`, `2 + 1`) that a
// later expression-folding pass could normalize but that are NOT redrawn
// here: unlike the pruned rules they are not dead weight (`x - x` is a
// legitimate way to spell 0), just non-canonical.
enum PruneRule {
    SelfAssign,             // v = v
    ArraySelfAssign,        // arr[e] = arr[e] / aux[e] = aux[e]
    SelfSwap,               // swap(e, e)
    ConstantCondition,      // if/while on a condition constant by its text
    AlwaysErrorOperand,     // Div/Mod by literal 0, Shl/Shr by a literal outside [0, 31]
    InfoConstantOperands,   // binop over two literals
    InfoSelfCancel,         // x - x, x ^ x
    InfoIdentityOperand,    // x + 0, 0 + x, x - 0, x * 1, 1 * x, x / 1, x | 0, x ^ 0, x << 0, x >> 0
    InfoAbsorbingOperand,   // x * 0, 0 * x, x & 0, 0 & x
    PruneRuleCount
};
const char* const pruneRuleNames[PruneRuleCount] = {
    "self_assign",       "array_self_assign", "self_swap",       "constant_condition", "always_error_operand",
    "info_const_operands", "info_self_cancel", "info_identity_operand", "info_absorbing_operand"};

std::atomic<long> gPruneHits[PruneRuleCount];
std::atomic<long> gPruneRedrawsExhausted{0};
std::atomic<long> gGenAssigns{0};       // denominators for the rates printed below
std::atomic<long> gGenArrayAssigns{0};
std::atomic<long> gGenSwaps{0};
std::atomic<long> gGenConds{0};
std::atomic<long> gGenBinops{0};

struct PruneDebugPrinter {
    ~PruneDebugPrinter() {
        if (std::getenv("ALGOSEARCH_DEBUG_PRUNE_COUNTS") == nullptr) return;
        const std::atomic<long>* denominators[PruneRuleCount] = {&gGenAssigns, &gGenArrayAssigns, &gGenSwaps,
                                                                 &gGenConds,   &gGenBinops,       &gGenBinops,
                                                                 &gGenBinops,  &gGenBinops,       &gGenBinops};
        for (int r = 0; r < PruneRuleCount; ++r) {
            long hits = gPruneHits[r].load();
            long total = denominators[r]->load();
            std::cerr << "[prune-debug] " << pruneRuleNames[r] << " hits=" << hits << " of=" << total << " rate="
                      << (total > 0 ? static_cast<double>(hits) / static_cast<double>(total) : 0.0) << "\n";
        }
        std::cerr << "[prune-debug] redraws_exhausted=" << gPruneRedrawsExhausted.load() << "\n";
    }
};
PruneDebugPrinter gPruneDebugPrinter;

// Upper bound on redraws per pruned site: a trivial draw is rare enough that
// this is effectively never exhausted, but it can never loop forever (e.g.
// numVars == 1 makes every Var-vs-Var draw a self-reference).
constexpr int maxPruneRedraws = 8;

// Checks an already-built site: if `isTrivial()` holds, counts it under
// `rule` and -- only when pruning is on -- redraws just `part` with `gen`
// (bounded) until it no longer does. Post-construction on purpose: the site
// is built exactly as before (including C++'s unspecified argument
// evaluation order in calls like makeBinOp(k, gen(), gen())), so with
// pruning off no extra RNG is consumed and the draw is untouched.
template <typename Gen, typename IsTrivial>
void prunePart(const GenConfig& cfg, PruneRule rule, ExprPtr& part, Gen gen, IsTrivial isTrivial) {
    if (!isTrivial()) return;
    gPruneHits[rule]++;
    if (!cfg.pruneGeneration) return;
    for (int attempt = 0; attempt < maxPruneRedraws && isTrivial(); ++attempt) part = gen();
    if (isTrivial()) gPruneRedrawsExhausted++;
}

bool isLiteral(const Expr& e, int value) { return e.kind == ExprKind::IntConst && e.intValue == value; }

bool isAlwaysErrorOperand(ExprKind k, const Expr& rhs) {
    if (rhs.kind != ExprKind::IntConst) return false;
    if (k == ExprKind::Div || k == ExprKind::Mod) return rhs.intValue == 0;
    if (k == ExprKind::Shl || k == ExprKind::Shr) return rhs.intValue < 0 || rhs.intValue >= 32;
    return false;
}

// INFO-only tally for a freshly built arithmetic/bitwise binop (never
// redrawn, see PruneRule above).
void tallyInfoPatterns(const Expr& e) {
    gGenBinops++;
    const Expr& l = *e.lhs;
    const Expr& r = *e.rhs;
    if (l.kind == ExprKind::IntConst && r.kind == ExprKind::IntConst) gPruneHits[InfoConstantOperands]++;
    if ((e.kind == ExprKind::Sub || e.kind == ExprKind::BitXor) && exprEqual(&l, &r)) gPruneHits[InfoSelfCancel]++;
    bool identity = false;
    switch (e.kind) {
        case ExprKind::Add:
        case ExprKind::BitOr: identity = isLiteral(l, 0) || isLiteral(r, 0); break;
        case ExprKind::BitXor:
        case ExprKind::Sub:
        case ExprKind::Shl:
        case ExprKind::Shr: identity = isLiteral(r, 0); break;
        case ExprKind::Mul: identity = isLiteral(l, 1) || isLiteral(r, 1); break;
        case ExprKind::Div: identity = isLiteral(r, 1); break;
        default: break;
    }
    if (identity) gPruneHits[InfoIdentityOperand]++;
    if ((e.kind == ExprKind::Mul || e.kind == ExprKind::BitAnd) && (isLiteral(l, 0) || isLiteral(r, 0))) {
        gPruneHits[InfoAbsorbingOperand]++;
    }
}

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
        ExprPtr e = makeBinOp(k, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
        prunePart(cfg, AlwaysErrorOperand, e->rhs, [&] { return genValueExpr(rng, depthBudget - 1, cfg); },
                  [&] { return isAlwaysErrorOperand(k, *e->rhs); });
        tallyInfoPatterns(*e);
        return e;
    }

    int choice = rng.uniformInt(0, 3);
    switch (choice) {
        case 0:
            if (cfg.secondArrayBias > 0.0 && rng.chance(cfg.secondArrayBias)) {
                return makeAuxAccess(genValueExpr(rng, depthBudget - 1, cfg));
            }
            return makeArrayAccess(genValueExpr(rng, depthBudget - 1, cfg));
        case 1: {
            ExprPtr e = makeBinOp(ExprKind::Add, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
            tallyInfoPatterns(*e);
            return e;
        }
        case 2: {
            ExprPtr e = makeBinOp(ExprKind::Sub, genValueExpr(rng, depthBudget - 1, cfg), genValueExpr(rng, depthBudget - 1, cfg));
            tallyInfoPatterns(*e);
            return e;
        }
        default:
            return makeVar(rng.uniformInt(0, cfg.numVars - 1));
    }
}

// Le/Ge/Ne are native ExprKinds (see ast/Expr.hpp), not emulated compounds.
// Sampled uniformly against Lt/Gt/Eq so every comparison is one grammar
// step away from any other, instead of forcing >=/<= through a fragile
// off-by-one rewrite (e.g. Gt(a, Sub(b,1))) or != through a compound
// (Lt(x,y)+Gt(x,y)).
ExprPtr genRawCondExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    ExprKind kinds[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    ExprKind k = kinds[rng.uniformInt(0, 5)];
    return makeCompare(k, genValueExpr(rng, depthBudget, cfg), genValueExpr(rng, depthBudget, cfg));
}

// A constant condition (see constantConditionValue) is redrawn whole, kind
// included: a constant-false if/while body never runs, a constant-true if is
// just its body with extra nodes, and a constant-true while is a guaranteed
// TIMEOUT unless something inside happens to break out.
ExprPtr genCondExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    gGenConds++;
    ExprPtr c = genRawCondExpr(rng, depthBudget, cfg);
    prunePart(cfg, ConstantCondition, c, [&] { return genRawCondExpr(rng, depthBudget, cfg); },
              [&] { return constantConditionValue(*c).has_value(); });
    return c;
}

std::vector<StmtPtr> genBlockStmts(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, int maxStmts,
                                    bool insideIf = false, bool insideWhile = false);

// Picks `n` distinct scratch-variable indices from [1, cfg.numVars-1]
// (var 0 is reserved -- the interpreter auto-seeds it with the array
// length, see Interpreter::run). Every multi-variable dedicated skeleton
// below used to inline this exact reroll-on-duplicate loop separately
// (13 byte-identical copies before this extraction, confirmed while
// cataloguing the skeleton library's primitives -- FINDINGS.md section
// 24); factored out once nothing else changes: same rng.uniformInt call
// sequence per call as every inlined copy had, so this is a pure
// deduplication, not a behavior change -- existing seeded results are
// unaffected. NOT used by genCountingLoopBlock's vi/acc pick or
// genRecurrenceSkeletonBlock's a/b pick, which resolve a collision via a
// modular remap instead of rerolling -- a genuinely different (cheaper,
// fixed-call-count) algorithm, not accidental duplication, so left
// untouched rather than folded in here.
std::vector<int> pickDistinctVars(Random& rng, const GenConfig& cfg, int n) {
    std::vector<int> vars;
    vars.reserve(static_cast<size_t>(n));
    while (static_cast<int>(vars.size()) < n) {
        int cand = 1 + rng.uniformInt(0, cfg.numVars - 2);
        bool dup = false;
        for (int existing : vars) {
            if (existing == cand) {
                dup = true;
                break;
            }
        }
        if (!dup) vars.push_back(cand);
    }
    return vars;
}

// Builds `base + (i * width + j)`, the flattened-2D-into-1D row-major
// index every skeleton needing a matrix/table (GraphReachability,
// ShortestPath, DtwProblem, TSP's edge matrix) hand-derived on its own
// because Mini-C has no native 2D array -- 10 call sites, byte-identical
// shape, confirmed while cataloguing the skeleton library's primitives
// (FINDINGS.md section 24). Pure expression construction, no RNG
// involved, so this carries none of pickDistinctVars' determinism risk:
// every call site below builds the exact same AST it built inline
// before, just via one shared function.
ExprPtr makeFlatIndex(ExprPtr base, ExprPtr i, ExprPtr width, ExprPtr j) {
    return makeBinOp(ExprKind::Add, std::move(base),
                      makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Mul, std::move(i), std::move(width)), std::move(j)));
}

// Appends `while (var < boundExpr) { <buildBody's statements>; var = var
// + 1; }` to `out` -- the loop half of the single most reused shape in
// this file (the "counting loop": an index bounds-checked and
// incremented exactly once per iteration), confirmed identical across
// SelectionSort/ThreeSum's every loop while cataloguing skeleton
// primitives (FINDINGS.md section 24). Deliberately does NOT also push
// `var = init` -- callers do that themselves wherever it belongs in
// their own statement order (which init goes where relative to OTHER
// pre-loop statements differs per skeleton, e.g. ThreeSum's `found = 0`
// sitting between `i = 0` and the i-loop), so this only dedups the loop
// itself, never silently reorders anything.
void appendCountingWhile(std::vector<StmtPtr>& out, int var, ExprPtr boundExpr, int& loopsUsed,
                          const std::function<void(std::vector<StmtPtr>&)>& buildBody) {
    std::vector<StmtPtr> body;
    buildBody(body);
    body.push_back(makeAssign(var, makeBinOp(ExprKind::Add, makeVar(var), makeIntConst(1))));
    loopsUsed++;
    out.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(var), std::move(boundExpr)), std::move(body)));
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
    std::vector<int> vars = pickDistinctVars(rng, cfg, 3);
    int i = vars[0], mi = vars[1], j = vars[2];

    auto root = makeBlock();
    root->body.push_back(makeAssign(i, makeIntConst(0)));  // i = 0
    appendCountingWhile(root->body, i, makeVar(0), loopsUsed, [&](std::vector<StmtPtr>& outerBody) {
        outerBody.push_back(makeAssign(mi, makeVar(i)));                                          // mi = i
        outerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));  // j = i + 1
        appendCountingWhile(outerBody, j, makeVar(0), loopsUsed, [&](std::vector<StmtPtr>& innerBody) {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(mi, makeVar(j)));
            innerBody.push_back(
                makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(makeVar(j)), makeArrayAccess(makeVar(mi))), std::move(then)));
        });
        outerBody.push_back(makeSwap(makeVar(i), makeVar(mi)));
    });

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
    std::vector<int> vars = pickDistinctVars(rng, cfg, 4);
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
// duty. Gated on numVars >= 5, same tier as the deleted NestedDivisor/Average.
StmtPtr genThreeSumSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> vars = pickDistinctVars(rng, cfg, 4);
    int i = vars[0], j = vars[1], k = vars[2], found = vars[3];

    auto root = makeBlock();
    root->body.push_back(makeAssign(i, makeIntConst(0)));      // i = 0
    root->body.push_back(makeAssign(found, makeIntConst(0)));  // found = 0
    appendCountingWhile(root->body, i, makeVar(0), loopsUsed, [&](std::vector<StmtPtr>& outerBody) {
        outerBody.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));  // j = i + 1
        appendCountingWhile(outerBody, j, makeVar(0), loopsUsed, [&](std::vector<StmtPtr>& midBody) {
            midBody.push_back(makeAssign(k, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));  // k = j + 1
            appendCountingWhile(midBody, k, makeVar(0), loopsUsed, [&](std::vector<StmtPtr>& innerBody) {
                auto sum = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeArrayAccess(makeVar(i)), makeArrayAccess(makeVar(j))),
                                      makeArrayAccess(makeVar(k)));
                std::vector<StmtPtr> then;
                then.push_back(makeAssign(found, makeIntConst(1)));
                innerBody.push_back(makeIf(makeCompare(ExprKind::Eq, std::move(sum), makeIntConst(0)), std::move(then)));
            });
        });
    });

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
    std::vector<int> vars = pickDistinctVars(rng, cfg, 5);
    int nv = vars[0], base = vars[1], round = vars[2], i = vars[3], j = vars[4];

    auto root = makeBlock();
    root->body.push_back(makeAssign(nv, makeArrayAccess(makeIntConst(0))));                       // nv = arr[0]
    root->body.push_back(makeAssignArray(makeIntConst(1), makeIntConst(1)));                      // arr[1] = 1
    root->body.push_back(makeAssign(base, makeBinOp(ExprKind::Add, makeVar(nv), makeIntConst(1))));  // base = nv + 1
    root->body.push_back(makeAssign(round, makeIntConst(0)));

    appendCountingWhile(root->body, round, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& roundBody) {
        roundBody.push_back(makeAssign(i, makeIntConst(0)));
        appendCountingWhile(roundBody, i, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& iBody) {
            std::vector<StmtPtr> ifReachedI;
            ifReachedI.push_back(makeAssign(j, makeIntConst(0)));
            appendCountingWhile(ifReachedI, j, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& jBody) {
                auto edgeIdx = makeFlatIndex(makeVar(base), makeVar(i), makeVar(nv), makeVar(j));
                std::vector<StmtPtr> markReached;
                markReached.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(j)), makeIntConst(1)));
                jBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeArrayAccess(std::move(edgeIdx)), makeIntConst(1)),
                                        std::move(markReached)));
            });
            iBody.push_back(makeIf(
                makeCompare(ExprKind::Eq, makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i))), makeIntConst(1)),
                std::move(ifReachedI)));
        });
    });

    root->body.push_back(makeAssignArray(makeIntConst(0), makeArrayAccess(makeVar(nv))));  // arr[0] = arr[nv] (reached[V-1])

    return root;
}

// Dedicated skeleton for ShortestPathProblem: Bellman-Ford-style relaxation
// on the same flat layout as GraphReachabilityProblem, but with weighted
// edges (0 = no edge) and a `dist` scratch array the program must
// initialize itself (dist[0]=0, dist[i]=INF otherwise) before relaxing.
//
//   nv = arr[0];
//   base = nv + 1;
//   i = 0;
//   while (i < nv) { arr[1+i] = 100000; i = i + 1; }   // dist[i] = INF
//   arr[1] = 0;                                        // dist[0] = 0
//   round = 0;
//   while (round < nv) {
//     i = 0;
//     while (i < nv) {
//       j = 0;
//       while (j < nv) {
//         w = arr[base + (i*nv+j)];
//         if (w > 0) {
//           if (arr[1+i] + w < arr[1+j]) { arr[1+j] = arr[1+i] + w; }
//         }
//         j = j + 1;
//       }
//       i = i + 1;
//     }
//     round = round + 1;
//   }
//   arr[0] = arr[nv];   // dist[V-1] lives at arr[1+(nv-1)] = arr[nv]
//
// Needs 6 scratch scalars (nv, base, round, i, j, w), so numVars >= 7, one
// tier up from GraphReachability's 6. Same triple-nested-loop budget gate.
StmtPtr genShortestPathSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> vars = pickDistinctVars(rng, cfg, 6);
    int nv = vars[0], base = vars[1], round = vars[2], i = vars[3], j = vars[4], w = vars[5];

    auto root = makeBlock();
    root->body.push_back(makeAssign(nv, makeArrayAccess(makeIntConst(0))));                          // nv = arr[0]
    root->body.push_back(makeAssign(base, makeBinOp(ExprKind::Add, makeVar(nv), makeIntConst(1))));  // base = nv + 1

    // dist[i] = INF for all i
    root->body.push_back(makeAssign(i, makeIntConst(0)));
    appendCountingWhile(root->body, i, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& initBody) {
        initBody.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i)), makeIntConst(100000)));
    });
    root->body.push_back(makeAssignArray(makeIntConst(1), makeIntConst(0)));  // dist[0] = 0

    root->body.push_back(makeAssign(round, makeIntConst(0)));
    appendCountingWhile(root->body, round, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& roundBody) {
        roundBody.push_back(makeAssign(i, makeIntConst(0)));
        appendCountingWhile(roundBody, i, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& iBody) {
            iBody.push_back(makeAssign(j, makeIntConst(0)));
            appendCountingWhile(iBody, j, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& jBody) {
                auto edgeIdx = makeFlatIndex(makeVar(base), makeVar(i), makeVar(nv), makeVar(j));
                jBody.push_back(makeAssign(w, makeArrayAccess(std::move(edgeIdx))));
                std::vector<StmtPtr> relax;
                auto candDist = makeBinOp(ExprKind::Add, makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i))), makeVar(w));
                std::vector<StmtPtr> update;
                update.push_back(makeAssignArray(
                    makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(j)),
                    makeBinOp(ExprKind::Add, makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i))), makeVar(w))));
                relax.push_back(makeIf(
                    makeCompare(ExprKind::Lt, std::move(candDist),
                                makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(j)))),
                    std::move(update)));
                jBody.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(w), makeIntConst(0)), std::move(relax)));
            });
        });
    });

    root->body.push_back(makeAssignArray(makeIntConst(0), makeArrayAccess(makeVar(nv))));  // arr[0] = arr[nv] (dist[V-1])

    return root;
}

// Dedicated skeleton for DtwProblem: the textbook O(n^2) global-alignment
// DP (see DtwProblem.hpp) on the same flattened-table convention as
// GraphReachability/ShortestPath, but unlike those two this needs no
// round/relaxation loop -- table[i][j] depends only on cells already
// computed (up, left, diag), so a single boundary-init pass plus one
// ordered double loop (i, then j, both increasing) fills it exactly once.
//
//   n = arr[0]; tw = n + 1;
//   base = 1 + tw*tw;      // start of sequence A
//   baseB = base + n;      // start of sequence B
//   i = 0;
//   while (i < tw) {                       // boundary: table[0][*] and table[*][0]
//     if (i == 0) { arr[1 + i*tw] = 0; }
//     else { arr[1 + i*tw] = INF; arr[1 + i] = INF; }
//     i = i + 1;
//   }
//   i = 1;
//   while (i < tw) {
//     j = 1;
//     while (j < tw) {
//       ai = arr[base - 1 + i]; bj = arr[baseB - 1 + j];
//       diff = ai - bj;
//       if (diff < 0) { cost = 0 - diff; } else { cost = diff; }
//       best = arr[1 + (i-1)*tw + j];                              // up
//       if (arr[1 + i*tw + (j-1)] < best) best = arr[1 + i*tw + (j-1)];      // left
//       if (arr[1 + (i-1)*tw + (j-1)] < best) best = arr[1 + (i-1)*tw + (j-1)];  // diag
//       arr[1 + i*tw + j] = cost + best;
//       j = j + 1;
//     }
//     i = i + 1;
//   }
//   arr[0] = arr[1 + n*tw + n];   // answer = table[n][n]
//
// Needs 11 scratch scalars (n, tw, base, baseB, i, j, ai, bj, diff, cost,
// best), so numVars >= 12, same tier as TSP. Three loops total (boundary +
// outer + inner), same canTripleLoop budget gate as ThreeSum/
// GraphReachability/ShortestPath.
StmtPtr genDtwSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> vars = pickDistinctVars(rng, cfg, 11);
    int n = vars[0], tw = vars[1], base = vars[2], baseB = vars[3], i = vars[4], j = vars[5], ai = vars[6],
        bj = vars[7], diff = vars[8], cost = vars[9], best = vars[10];

    auto root = makeBlock();
    root->body.push_back(makeAssign(n, makeArrayAccess(makeIntConst(0))));  // n = arr[0]
    root->body.push_back(makeAssign(tw, makeBinOp(ExprKind::Add, makeVar(n), makeIntConst(1))));  // tw = n + 1
    root->body.push_back(
        makeAssign(base, makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeVar(tw), makeVar(tw)))));
    root->body.push_back(makeAssign(baseB, makeBinOp(ExprKind::Add, makeVar(base), makeVar(n))));

    // Boundary init: table[0][0]=0, table[i][0]=table[0][i]=INF for i>0.
    root->body.push_back(makeAssign(i, makeIntConst(0)));
    appendCountingWhile(root->body, i, makeVar(tw), loopsUsed, [&](std::vector<StmtPtr>& initBody) {
        auto col0 = makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeVar(i), makeVar(tw)));
        std::vector<StmtPtr> zeroBranch;
        zeroBranch.push_back(makeAssignArray(std::move(col0), makeIntConst(0)));
        std::vector<StmtPtr> infBranch;
        auto col0b = makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeVar(i), makeVar(tw)));
        infBranch.push_back(makeAssignArray(std::move(col0b), makeIntConst(100000)));
        infBranch.push_back(
            makeAssignArray(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i)), makeIntConst(100000)));
        initBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(i), makeIntConst(0)), std::move(zeroBranch),
                                   std::move(infBranch)));
    });

    root->body.push_back(makeAssign(i, makeIntConst(1)));
    appendCountingWhile(root->body, i, makeVar(tw), loopsUsed, [&](std::vector<StmtPtr>& iBody) {
        iBody.push_back(makeAssign(j, makeIntConst(1)));
        appendCountingWhile(iBody, j, makeVar(tw), loopsUsed, [&](std::vector<StmtPtr>& jBody) {
            jBody.push_back(
                makeAssign(ai, makeArrayAccess(makeBinOp(ExprKind::Sub, makeBinOp(ExprKind::Add, makeVar(base), makeVar(i)),
                                                          makeIntConst(1)))));
            jBody.push_back(makeAssign(
                bj, makeArrayAccess(
                        makeBinOp(ExprKind::Sub, makeBinOp(ExprKind::Add, makeVar(baseB), makeVar(j)), makeIntConst(1)))));
            jBody.push_back(makeAssign(diff, makeBinOp(ExprKind::Sub, makeVar(ai), makeVar(bj))));
            {
                std::vector<StmtPtr> negBranch;
                negBranch.push_back(makeAssign(cost, makeBinOp(ExprKind::Sub, makeIntConst(0), makeVar(diff))));
                std::vector<StmtPtr> posBranch;
                posBranch.push_back(makeAssign(cost, makeVar(diff)));
                jBody.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(diff), makeIntConst(0)), std::move(negBranch),
                                        std::move(posBranch)));
            }
            auto iMinus1 = makeBinOp(ExprKind::Sub, makeVar(i), makeIntConst(1));
            auto jMinus1 = makeBinOp(ExprKind::Sub, makeVar(j), makeIntConst(1));
            auto upIdx = makeFlatIndex(makeIntConst(1), iMinus1->clone(), makeVar(tw), makeVar(j));
            jBody.push_back(makeAssign(best, makeArrayAccess(std::move(upIdx))));
            auto leftIdx = makeFlatIndex(makeIntConst(1), makeVar(i), makeVar(tw), jMinus1->clone());
            {
                std::vector<StmtPtr> updateLeft;
                auto leftIdx2 = makeFlatIndex(makeIntConst(1), makeVar(i), makeVar(tw), jMinus1->clone());
                updateLeft.push_back(makeAssign(best, makeArrayAccess(std::move(leftIdx2))));
                jBody.push_back(
                    makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(std::move(leftIdx)), makeVar(best)), std::move(updateLeft)));
            }
            auto diagIdx = makeFlatIndex(makeIntConst(1), iMinus1->clone(), makeVar(tw), jMinus1->clone());
            {
                std::vector<StmtPtr> updateDiag;
                auto diagIdx2 = makeFlatIndex(makeIntConst(1), iMinus1->clone(), makeVar(tw), jMinus1->clone());
                updateDiag.push_back(makeAssign(best, makeArrayAccess(std::move(diagIdx2))));
                jBody.push_back(
                    makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(std::move(diagIdx)), makeVar(best)), std::move(updateDiag)));
            }
            auto cellIdx = makeFlatIndex(makeIntConst(1), makeVar(i), makeVar(tw), makeVar(j));
            jBody.push_back(makeAssignArray(std::move(cellIdx), makeBinOp(ExprKind::Add, makeVar(cost), makeVar(best))));
        });
    });

    auto finalIdx = makeFlatIndex(makeIntConst(1), makeVar(n), makeVar(tw), makeVar(n));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeArrayAccess(std::move(finalIdx))));  // arr[0] = table[n][n]

    return root;
}

// EXPERIMENT-ONLY, staged skeleton for the "does a converging two-pointer
// scan let free search complete binary search itself?" question (FINDINGS.md
// section 20/21: contains_ordered stays a plain linear scan even with
// budget/diversity/quotas maxed out). Direction-aware binary search over a
// haystack that may be ascending OR descending (ContainsOrderedProblem's
// data is never only one direction):
//   target = arr[0]; lo = 1; hi = v0 (auto array length); found = 0;
//   dir = 1; if (arr[1] > arr[v0-1]) dir = -1;      // always hand-written
//   while (lo < hi) {
//     mid = lo + (hi - lo) / 2; v = arr[mid];
//     if (v == target) { found = 1; hi = lo; }       // forces loop exit
//     else { diff = (v - target) * dir; if (diff < 0) lo = mid+1; else hi = mid; }
//   }
//   arr[0] = found;
//
// `ALGOSEARCH_BSEARCH_STAGE` (1-4, default 4) controls how much of the
// LOOP BODY is hand-written vs. left as a free-generated "hole" (same
// technique as TSP's `guardAsHole`, generalized into a ladder instead of
// one on/off flag), so a sweep across stages finds the exact tipping
// point where free search can complete the rest on its own:
//   1: only the converging-loop FRAME (lo/hi init, while(lo<hi), the
//      final arr[0]=found) is hand-written; the entire loop body,
//      including computing a midpoint at all, is free-generated.
//   2: frame + mid/v computation hand-written; the found-check and
//      lo/hi-movement decision is free-generated.
//   3: frame + mid/v + the found-check/branch STRUCTURE hand-written;
//      only the final "which way to move" comparison (the direct analog
//      of TSP's `cost < best` hole) is a free-generated condition.
//   4: fully hand-written, no holes -- a sanity check that the complete
//      algorithm itself converges/generalizes via real search, the same
//      role ALGOSEARCH_FORCE_SKELETON played for genDtwSkeletonBlock.
// Registered under the name "binarysearch" for
// ALGOSEARCH_FORCE_SKELETON/ALGOSEARCH_DISABLE_SKELETON. Needs 8 scratch
// scalars (target, lo, hi, mid, v, dir, diff, found), numVars >= 9; only
// ONE loop (no relaxation rounds needed), so canLoop is enough.
StmtPtr genBinarySearchSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    int stage = 4;
    if (const char* s = std::getenv("ALGOSEARCH_BSEARCH_STAGE")) {
        stage = std::atoi(s);
        if (stage < 1) stage = 1;
        if (stage > 4) stage = 4;
    }

    std::vector<int> vars = pickDistinctVars(rng, cfg, 8);
    int target = vars[0], lo = vars[1], hi = vars[2], mid = vars[3], v = vars[4], dir = vars[5], diff = vars[6],
        found = vars[7];

    auto root = makeBlock();
    root->body.push_back(makeAssign(target, makeArrayAccess(makeIntConst(0))));
    root->body.push_back(makeAssign(lo, makeIntConst(1)));
    root->body.push_back(makeAssign(hi, makeVar(0)));  // v0 = auto array length
    root->body.push_back(makeAssign(found, makeIntConst(0)));

    // Direction detection: always hand-written at every stage -- not the
    // object of study here, and without it there is no single correct
    // comparison direction for the free-generated parts to land on even
    // in principle.
    root->body.push_back(makeAssign(dir, makeIntConst(1)));
    {
        std::vector<StmtPtr> flip;
        flip.push_back(makeAssign(dir, makeBinOp(ExprKind::Sub, makeIntConst(0), makeIntConst(1))));
        auto firstVsLast = makeCompare(ExprKind::Gt, makeArrayAccess(makeIntConst(1)),
                                        makeArrayAccess(makeBinOp(ExprKind::Sub, makeVar(0), makeIntConst(1))));
        root->body.push_back(makeIf(std::move(firstVsLast), std::move(flip)));
    }

    // Holes are filled via genBlockStmts using a LOCAL, already-maxed-out
    // loop counter (never the real `loopsUsed`) so canLoop is false inside
    // them: without this, the loopAccumulatorBias trigger inside a hole
    // can re-select this very skeleton (ALGOSEARCH_FORCE_SKELETON pins the
    // choice) and recurse without bound -- confirmed as a real stack
    // overflow (genStmt -> genBlockStmts -> this function -> genStmt ->
    // ...) before this guard was added. Holes may still contain If/Assign/
    // Swap (canNest only needs depthBudget>0), just no new While loops --
    // appropriate anyway, since a hole here stands in for DECISION logic
    // inside the loop this skeleton already built, not a second loop.
    int holeLoopsUsed = cfg.maxLoops;
    std::vector<StmtPtr> loopBody;
    if (stage == 1) {
        loopBody =
            genBlockStmts(rng, 2, cfg, holeLoopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/false, /*insideWhile=*/true);
    } else {
        loopBody.push_back(makeAssign(
            mid, makeBinOp(ExprKind::Add, makeVar(lo),
                            makeBinOp(ExprKind::Div, makeBinOp(ExprKind::Sub, makeVar(hi), makeVar(lo)), makeIntConst(2)))));
        loopBody.push_back(makeAssign(v, makeArrayAccess(makeVar(mid))));
        if (stage == 2) {
            auto rest = genBlockStmts(rng, 2, cfg, holeLoopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/false, /*insideWhile=*/true);
            for (auto& s : rest) loopBody.push_back(std::move(s));
        } else {
            std::vector<StmtPtr> foundBranch;
            foundBranch.push_back(makeAssign(found, makeIntConst(1)));
            foundBranch.push_back(makeAssign(hi, makeVar(lo)));  // force loop exit next check

            std::vector<StmtPtr> elseBranch;
            if (stage == 3) {
                // The decision itself (which analog of TSP's `cost < best`
                // hole) is the hole: a free-generated condition gates
                // free-generated lo/hi updates.
                auto thenBody = genBlockStmts(rng, 2, cfg, holeLoopsUsed, 2, /*insideIf=*/true, /*insideWhile=*/true);
                auto elseBody2 = genBlockStmts(rng, 2, cfg, holeLoopsUsed, 2, /*insideIf=*/true, /*insideWhile=*/true);
                elseBranch.push_back(makeIf(genCondExpr(rng, cfg.maxExprDepth, cfg), std::move(thenBody), std::move(elseBody2)));
            } else {  // stage 4: fully hand-written
                elseBranch.push_back(
                    makeAssign(diff, makeBinOp(ExprKind::Mul, makeBinOp(ExprKind::Sub, makeVar(v), makeVar(target)), makeVar(dir))));
                std::vector<StmtPtr> moveRight;
                moveRight.push_back(makeAssign(lo, makeBinOp(ExprKind::Add, makeVar(mid), makeIntConst(1))));
                std::vector<StmtPtr> moveLeft;
                moveLeft.push_back(makeAssign(hi, makeVar(mid)));
                elseBranch.push_back(
                    makeIf(makeCompare(ExprKind::Lt, makeVar(diff), makeIntConst(0)), std::move(moveRight), std::move(moveLeft)));
            }
            loopBody.push_back(
                makeIf(makeCompare(ExprKind::Eq, makeVar(v), makeVar(target)), std::move(foundBranch), std::move(elseBranch)));
        }
    }
    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(lo), makeVar(hi)), std::move(loopBody)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(found)));

    return root;
}

// Dedicated skeleton for TspProblem: exact brute-force TSP via Heap's
// algorithm, run ITERATIVELY (no recursion -- Mini-C has none) over the
// permutations of vertices 1..V-1 (vertex 0 is fixed as the tour's
// start/end), keeping the minimum-cost valid tour seen. Unlike every other
// dedicated skeleton in this file, this is deliberately exponential
// ((V-1)! permutations) -- there is no polynomial fixed-point trick for
// exact TSP (the "visit each vertex once" constraint isn't monotone the
// way reachability/shortest-path's relaxation is), so brute force is the
// only exact option, and the whole point of this skeleton is to test
// whether the search can find genuine brute-force enumeration at all, not
// to find something smarter than brute force.
//
// Mini-C layout (see TspProblem.hpp): arr[0]=V; arr[1..2*(V-1)] is scratch
// -- perm[k] at arr[1+k] (a permutation of vertices 1..V-1) and Heap's
// counter c[k] at arr[1+size+k], where size=V-1; arr[1+2*size..] is the
// row-major V*V weight matrix (0 = no edge). Needs 11 scratch scalars
// (nv, size, base, k, i, t, cost, w, prevVertex, valid, best), so
// numVars >= 12.
//
// Iterative Heap's algorithm (classic non-recursive form): c[] all start
// at 0; repeatedly, at index i, if c[i] < i swap perm[0] (i even) or
// perm[c[i]] (i odd) with perm[i], evaluate the new permutation, bump
// c[i], and restart i at 0; otherwise reset c[i] and advance i. This
// visits all (V-1)! permutations exactly once, using only scalar state
// and array swaps -- no recursion, no dynamic-size structures.
StmtPtr genTspBruteForceSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> vars = pickDistinctVars(rng, cfg, 11);
    int nv = vars[0], size = vars[1], base = vars[2], k = vars[3], i = vars[4], t = vars[5], cost = vars[6],
        w = vars[7], prevVertex = vars[8], valid = vars[9], best = vars[10];

    // Builds (fresh each call -- AST nodes are move-only) the "compute the
    // current permutation's tour cost, then update `best` if it's a valid
    // and cheaper tour" block. Reused for the identity permutation and
    // after every Heap's-algorithm swap.
    auto buildCostAndUpdateBest = [&]() {
        std::vector<StmtPtr> stmts;
        stmts.push_back(makeAssign(t, makeIntConst(0)));
        stmts.push_back(makeAssign(cost, makeIntConst(0)));
        stmts.push_back(makeAssign(prevVertex, makeIntConst(0)));
        stmts.push_back(makeAssign(valid, makeIntConst(1)));
        appendCountingWhile(stmts, t, makeVar(size), loopsUsed, [&](std::vector<StmtPtr>& body) {
            auto edgeIdx = makeFlatIndex(makeVar(base), makeVar(prevVertex), makeVar(nv),
                                          makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(t))));
            body.push_back(makeAssign(w, makeArrayAccess(std::move(edgeIdx))));
            std::vector<StmtPtr> invalidate;
            invalidate.push_back(makeAssign(valid, makeIntConst(0)));
            body.push_back(makeIf(makeCompare(ExprKind::Le, makeVar(w), makeIntConst(0)), std::move(invalidate)));
            body.push_back(makeAssign(cost, makeBinOp(ExprKind::Add, makeVar(cost), makeVar(w))));
            body.push_back(makeAssign(prevVertex, makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(t)))));
        });
        // Closing edge back to vertex 0.
        {
            auto edgeIdx = makeFlatIndex(makeVar(base), makeVar(prevVertex), makeVar(nv), makeIntConst(0));
            stmts.push_back(makeAssign(w, makeArrayAccess(std::move(edgeIdx))));
            std::vector<StmtPtr> invalidate;
            invalidate.push_back(makeAssign(valid, makeIntConst(0)));
            stmts.push_back(makeIf(makeCompare(ExprKind::Le, makeVar(w), makeIntConst(0)), std::move(invalidate)));
            stmts.push_back(makeAssign(cost, makeBinOp(ExprKind::Add, makeVar(cost), makeVar(w))));
        }
        {
            std::vector<StmtPtr> updateBest;
            updateBest.push_back(makeAssign(best, makeVar(cost)));
            std::vector<StmtPtr> checkCheaper;
            checkCheaper.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(cost), makeVar(best)), std::move(updateBest)));
            stmts.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(valid), makeIntConst(0)), std::move(checkCheaper)));
        }
        return stmts;
    };

    auto root = makeBlock();
    root->body.push_back(makeAssign(nv, makeArrayAccess(makeIntConst(0))));                          // nv = arr[0]
    root->body.push_back(makeAssign(size, makeBinOp(ExprKind::Sub, makeVar(nv), makeIntConst(1))));   // size = nv - 1
    // base = 1 + 4*nv -- matches TspProblem's unified scratch region (see
    // TspProblem.hpp), sized for the branch-and-bound skeleton's larger
    // four-array layout; this skeleton only uses the first 2*size cells
    // of it (perm[]/c[]), leaving the rest unused but zero-initialized.
    root->body.push_back(
        makeAssign(base, makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeIntConst(4), makeVar(nv)))));
    root->body.push_back(makeAssign(best, makeIntConst(100000)));                                     // best = INF sentinel

    // V == 1: trivial single-vertex tour, cost 0 -- no permutation to try.
    std::vector<StmtPtr> trivialBranch;
    trivialBranch.push_back(makeAssignArray(makeIntConst(0), makeIntConst(0)));

    std::vector<StmtPtr> mainBranch;
    // perm[k] = k+1 (identity permutation of vertices 1..V-1); c[k] = 0.
    mainBranch.push_back(makeAssign(k, makeIntConst(0)));
    appendCountingWhile(mainBranch, k, makeVar(size), loopsUsed, [&](std::vector<StmtPtr>& initBody) {
        initBody.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(k)),
                                            makeBinOp(ExprKind::Add, makeVar(k), makeIntConst(1))));
        initBody.push_back(makeAssignArray(
            makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(size)), makeVar(k)), makeIntConst(0)));
    });

    // Evaluate the identity permutation before entering Heap's algorithm.
    for (auto& s : buildCostAndUpdateBest()) mainBranch.push_back(std::move(s));

    // Iterative Heap's algorithm: enumerate every remaining permutation.
    mainBranch.push_back(makeAssign(i, makeIntConst(0)));
    {
        std::vector<StmtPtr> heapBody;
        auto cAtI = makeArrayAccess(
            makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(size)), makeVar(i)));
        std::vector<StmtPtr> thenBranch;  // c[i] < i
        {
            std::vector<StmtPtr> evenSwap;
            evenSwap.push_back(
                makeSwap(makeIntConst(1), makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i))));  // perm[0] <-> perm[i]
            std::vector<StmtPtr> oddSwap;
            auto cAtI2 = makeArrayAccess(
                makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(size)), makeVar(i)));
            oddSwap.push_back(makeSwap(makeBinOp(ExprKind::Add, makeIntConst(1), std::move(cAtI2)),
                                        makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(i))));  // perm[c[i]] <-> perm[i]
            thenBranch.push_back(makeIf(
                makeCompare(ExprKind::Eq, makeBinOp(ExprKind::Mod, makeVar(i), makeIntConst(2)), makeIntConst(0)),
                std::move(evenSwap), std::move(oddSwap)));
            for (auto& s : buildCostAndUpdateBest()) thenBranch.push_back(std::move(s));
            auto cAtI3 = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(size)), makeVar(i));
            auto cAtI3Idx = cAtI3->clone();
            auto cAtI3Read = makeArrayAccess(std::move(cAtI3));
            thenBranch.push_back(
                makeAssignArray(std::move(cAtI3Idx), makeBinOp(ExprKind::Add, std::move(cAtI3Read), makeIntConst(1))));
            thenBranch.push_back(makeAssign(i, makeIntConst(0)));
        }
        std::vector<StmtPtr> elseBranch;  // c[i] = 0; i = i + 1
        {
            auto cAtI4 = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(size)), makeVar(i));
            elseBranch.push_back(makeAssignArray(std::move(cAtI4), makeIntConst(0)));
            elseBranch.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));
        }
        heapBody.push_back(
            makeIf(makeCompare(ExprKind::Lt, std::move(cAtI), makeVar(i)), std::move(thenBranch), std::move(elseBranch)));
        loopsUsed++;
        mainBranch.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeVar(size)), std::move(heapBody)));
    }

    mainBranch.push_back(makeAssignArray(makeIntConst(0), makeVar(best)));
    std::vector<StmtPtr> noTour;
    noTour.push_back(makeAssignArray(makeIntConst(0), makeIntConst(-1)));
    mainBranch.push_back(makeIf(makeCompare(ExprKind::Ge, makeVar(best), makeIntConst(100000)), std::move(noTour)));

    root->body.push_back(
        makeIf(makeCompare(ExprKind::Le, makeVar(size), makeIntConst(0)), std::move(trivialBranch), std::move(mainBranch)));

    return root;
}

// Second, more advanced TSP skeleton: exact branch-and-bound, iterative DFS
// (no recursion -- Mini-C has none) with an explicit backtracking stack
// held in arrays, PLUS pruning (cut a partial path the moment its cost
// already reaches the best complete tour found so far). Unlike
// genTspBruteForceSkeletonBlock's Heap's-algorithm enumeration (which
// always visits all (V-1)! permutations, no matter how bad a partial path
// looks), this can skip huge swaths of the search tree once a decent
// `best` is found -- same worst-case complexity, but a genuinely different
// (and typically much faster in practice) constant/average case. Built to
// A/B against the brute-force skeleton and answer "does pruning actually
// help" empirically rather than by assumption.
//
// Mini-C layout is the SAME as TspProblem's brute-force encoding (see
// TspProblem.hpp) -- V, then 2*(V-1) scratch cells, then the V*V weight
// matrix -- but this skeleton repurposes/extends the scratch region into
// FOUR parallel arrays of length V instead of two of length (V-1):
//   path[k]         at arr[1+k]              -- path[0]=0 fixed; path[1..
//                                                size] is the partial/full
//                                                tour being built.
//   used[k]         at arr[1+nv+k]           -- 1 if vertex k is already
//                                                in the current partial
//                                                path, else 0.
//   choice[k]       at arr[1+2*nv+k]         -- choice[level] is the next
//                                                candidate vertex to try
//                                                at DFS depth `level`
//                                                (the iterative
//                                                equivalent of a
//                                                recursive call's local
//                                                loop variable).
//   partialCost[k]  at arr[1+3*nv+k]         -- partialCost[level] is the
//                                                accumulated edge weight
//                                                of path[0..level].
// This needs V cells per array (4*V total) rather than the brute-force
// skeleton's 2*(V-1) -- more scratch than TspProblem's own doc comment
// reserves, but harmlessly so: the extra cells simply overlap into what
// would otherwise be unused padding since 4*V > 2*(V-1) only by a small,
// bounded margin, and every index used here is still < 1+4*nv, safely
// before the matrix -- TspProblem.hpp's scratch-region doc comment should
// be read as "at least this much", not "exactly this much".
//
// Needs only 2 while loops (an init loop, then ONE flat DFS loop -- no
// recursion means no separate loop per depth, just an explicit `level`
// variable the single loop pushes/pops) and 11 scratch scalars (nv, size,
// uBase, chBase, pcBase, mBase, best, level, c, w, cost), one fewer loop
// than the brute-force skeleton's 4, so gated on canNestedLoop rather than
// canQuadLoop.
// (A `guardAsHole` variant of this skeleton -- replacing the `cost <
// best` line with a randomly-generated hole, to test whether the DFS
// shape alone is enough scaffolding for evolution to discover a working
// guard on its own -- was built and tested as Test 2 in
// data/skeleton_middle_ground_experiments.md. Conclusion: the shape is
// reliably learnable but the hole always converges to a vacuous
// tautology, never real pruning; removed after the experiment concluded,
// see FINDINGS.md section 18.)
// Builds the branch-and-bound program with an arbitrary pruning `guard` in
// place of `cost < best` (section 41's hole filling enumerates candidates for
// it), and reports the guard's If node so its evaluations can be traced.
TspBranchAndBound buildTspBranchAndBoundImpl(const std::vector<int>& vars, ExprPtr guard, int& loopsUsed) {
    const Stmt* guardSite = nullptr;
    int nv = vars[0], size = vars[1], uBase = vars[2], chBase = vars[3], pcBase = vars[4], mBase = vars[5],
        best = vars[6], level = vars[7], c = vars[8], w = vars[9], cost = vars[10];

    auto root = makeBlock();
    root->body.push_back(makeAssign(nv, makeArrayAccess(makeIntConst(0))));                        // nv = arr[0]
    root->body.push_back(makeAssign(size, makeBinOp(ExprKind::Sub, makeVar(nv), makeIntConst(1))));  // size = nv - 1
    root->body.push_back(makeAssign(uBase, makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(nv))));  // uBase = 1 + nv
    root->body.push_back(makeAssign(
        chBase, makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeIntConst(2), makeVar(nv)))));  // 1+2nv
    root->body.push_back(makeAssign(
        pcBase, makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeIntConst(3), makeVar(nv)))));  // 1+3nv
    root->body.push_back(makeAssign(
        mBase, makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Mul, makeIntConst(4), makeVar(nv)))));   // 1+4nv
    root->body.push_back(makeAssign(best, makeIntConst(100000)));                                    // best = INF sentinel

    // V == 1: trivial single-vertex tour, cost 0 -- no path to search.
    std::vector<StmtPtr> trivialBranch;
    trivialBranch.push_back(makeAssignArray(makeIntConst(0), makeIntConst(0)));

    std::vector<StmtPtr> mainBranch;
    // used[k] = 0 for all k in [0, nv) -- reuses `c` as the init counter,
    // safe since it's overwritten with its real meaning before the main
    // loop ever reads it.
    mainBranch.push_back(makeAssign(c, makeIntConst(0)));
    appendCountingWhile(mainBranch, c, makeVar(nv), loopsUsed, [&](std::vector<StmtPtr>& initBody) {
        initBody.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeVar(uBase), makeVar(c)), makeIntConst(0)));
    });
    mainBranch.push_back(makeAssignArray(makeIntConst(1), makeIntConst(0)));                     // path[0] = 0
    mainBranch.push_back(makeAssignArray(makeVar(uBase), makeIntConst(1)));                       // used[0] = 1
    mainBranch.push_back(makeAssignArray(makeVar(pcBase), makeIntConst(0)));                      // partialCost[0] = 0
    mainBranch.push_back(makeAssign(level, makeIntConst(1)));                                     // level = 1
    mainBranch.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeVar(chBase), makeVar(level)), makeIntConst(1)));  // choice[1] = 1

    // Iterative DFS with pruning: the single flat loop IS the whole call
    // stack -- `level` is the current depth, and the array reads/writes
    // below are exactly what a recursive solve(level) would keep on its
    // stack frame.
    {
        std::vector<StmtPtr> loopBody;
        auto choiceExpr = makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(chBase), makeVar(level)));

        // Branch A: choice[level] > size -- no candidates left at this
        // depth. Backtrack: free the vertex placed here and go up one
        // level, or (if level==1) terminate the whole search by driving
        // `level` to 0, which fails the outer while's `level >= 1` guard.
        std::vector<StmtPtr> exhaustedBranch;
        {
            std::vector<StmtPtr> terminateBranch;
            terminateBranch.push_back(makeAssign(level, makeIntConst(0)));
            // Undo the PARENT's placement, not this (exhausted) level's own
            // -- this level never leaves anything of its own to undo here
            // (every candidate it placed was already undone right after
            // use, in notPrunedBranch/leafBranch). The vertex that needs
            // freeing is the one the parent placed at path[level-1] before
            // descending into this level, so decrement `level` FIRST and
            // only then read path[level] (now the parent's slot). Reading
            // path[level] before decrementing (the original, buggy order)
            // reads this level's own slot instead -- stale/never-written
            // whenever every candidate here was skipped (used or invalid
            // edge) without ever placing one, corrupting `used[]` with
            // garbage (confirmed directly: it incorrectly zeroed used[0],
            // unmarking the permanently-used start vertex, causing missed
            // valid tours -- e.g. V=3 with edges 0->1,0->2,1->0,2->0,2->1
            // wrongly returned -1 instead of the real answer 12).
            std::vector<StmtPtr> backtrackBranch;
            backtrackBranch.push_back(makeAssign(level, makeBinOp(ExprKind::Sub, makeVar(level), makeIntConst(1))));
            backtrackBranch.push_back(makeAssign(c, makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(level)))));  // c = path[level] (parent's slot)
            backtrackBranch.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeVar(uBase), makeVar(c)), makeIntConst(0)));      // used[c] = 0
            exhaustedBranch.push_back(
                makeIf(makeCompare(ExprKind::Eq, makeVar(level), makeIntConst(1)), std::move(terminateBranch), std::move(backtrackBranch)));
        }

        // Branch B: a candidate remains -- try it, with pruning.
        std::vector<StmtPtr> tryBranch;
        {
            tryBranch.push_back(makeAssign(c, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(chBase), makeVar(level)))));  // c = choice[level]
            tryBranch.push_back(makeAssignArray(
                makeBinOp(ExprKind::Add, makeVar(chBase), makeVar(level)),
                makeBinOp(ExprKind::Add, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(chBase), makeVar(level))), makeIntConst(1))));  // choice[level] += 1

            std::vector<StmtPtr> notUsedBranch;  // used[c] == 0
            {
                auto edgeIdx = makeFlatIndex(
                    makeVar(mBase),
                    makeArrayAccess(makeBinOp(ExprKind::Add, makeIntConst(1), makeBinOp(ExprKind::Sub, makeVar(level), makeIntConst(1)))),
                    makeVar(nv), makeVar(c));
                notUsedBranch.push_back(makeAssign(w, makeArrayAccess(std::move(edgeIdx))));  // w = weight(path[level-1], c)

                std::vector<StmtPtr> validEdgeBranch;  // w > 0
                {
                    validEdgeBranch.push_back(makeAssign(
                        cost, makeBinOp(ExprKind::Add,
                                        makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(pcBase),
                                                                   makeBinOp(ExprKind::Sub, makeVar(level), makeIntConst(1)))),
                                        makeVar(w))));  // cost = partialCost[level-1] + w

                    std::vector<StmtPtr> notPrunedBranch;  // cost < best -- worth exploring
                    {
                        notPrunedBranch.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeIntConst(1), makeVar(level)), makeVar(c)));  // path[level] = c
                        notPrunedBranch.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeVar(uBase), makeVar(c)), makeIntConst(1)));  // used[c] = 1
                        notPrunedBranch.push_back(
                            makeAssignArray(makeBinOp(ExprKind::Add, makeVar(pcBase), makeVar(level)), makeVar(cost)));  // partialCost[level] = cost

                        std::vector<StmtPtr> leafBranch;  // level == size: complete tour, close the cycle
                        {
                            auto closeIdx = makeFlatIndex(makeVar(mBase), makeVar(c), makeVar(nv), makeIntConst(0));
                            leafBranch.push_back(makeAssign(w, makeArrayAccess(std::move(closeIdx))));  // w = weight(c, 0)
                            std::vector<StmtPtr> closeValidBranch;                                      // w > 0: valid closing edge
                            {
                                std::vector<StmtPtr> betterBranch;
                                betterBranch.push_back(makeAssign(best, makeBinOp(ExprKind::Add, makeVar(cost), makeVar(w))));
                                closeValidBranch.push_back(makeIf(
                                    makeCompare(ExprKind::Lt, makeBinOp(ExprKind::Add, makeVar(cost), makeVar(w)), makeVar(best)),
                                    std::move(betterBranch)));
                            }
                            leafBranch.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(w), makeIntConst(0)), std::move(closeValidBranch)));
                            leafBranch.push_back(makeAssignArray(makeBinOp(ExprKind::Add, makeVar(uBase), makeVar(c)), makeIntConst(0)));  // used[c]=0, undo immediately (leaf, no descend)
                        }
                        std::vector<StmtPtr> descendBranch;  // level < size: go one level deeper
                        {
                            descendBranch.push_back(makeAssign(level, makeBinOp(ExprKind::Add, makeVar(level), makeIntConst(1))));
                            descendBranch.push_back(makeAssignArray(
                                makeBinOp(ExprKind::Add, makeVar(chBase), makeVar(level)), makeIntConst(1)));  // choice[level] = 1
                        }
                        notPrunedBranch.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(level), makeVar(size)), std::move(leafBranch),
                                                          std::move(descendBranch)));
                    }
                    StmtPtr guardIf = makeIf(std::move(guard), std::move(notPrunedBranch));
                    guardSite = guardIf.get();
                    validEdgeBranch.push_back(std::move(guardIf));
                }
                notUsedBranch.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(w), makeIntConst(0)), std::move(validEdgeBranch)));
            }
            tryBranch.push_back(makeIf(
                makeCompare(ExprKind::Eq, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(uBase), makeVar(c))), makeIntConst(0)),
                std::move(notUsedBranch)));
        }

        loopBody.push_back(makeIf(makeCompare(ExprKind::Gt, std::move(choiceExpr), makeVar(size)), std::move(exhaustedBranch),
                                   std::move(tryBranch)));
        loopsUsed++;
        mainBranch.push_back(makeWhile(makeCompare(ExprKind::Ge, makeVar(level), makeIntConst(1)), std::move(loopBody)));
    }

    mainBranch.push_back(makeAssignArray(makeIntConst(0), makeVar(best)));
    std::vector<StmtPtr> noTour;
    noTour.push_back(makeAssignArray(makeIntConst(0), makeIntConst(-1)));
    mainBranch.push_back(makeIf(makeCompare(ExprKind::Ge, makeVar(best), makeIntConst(100000)), std::move(noTour)));

    root->body.push_back(
        makeIf(makeCompare(ExprKind::Le, makeVar(size), makeIntConst(0)), std::move(trivialBranch), std::move(mainBranch)));

    return {std::move(root), guardSite};
}

StmtPtr genTspBranchAndBoundSkeletonBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> vars = pickDistinctVars(rng, cfg, 11);
    int cost = vars[10], best = vars[6];
    return buildTspBranchAndBoundImpl(vars, makeCompare(ExprKind::Lt, makeVar(cost), makeVar(best)), loopsUsed).program;
}

// Fully hand-written merge sort, the same status as genSelectionSortSkeletonBlock/
// genHeapSortSkeletonBlock: a control to answer "can the language and the
// search USE a correct recursive template at all", separate from
// genDivideConquerSkeletonBlock's open question of whether the search can
// DISCOVER the combine step itself (which the forced-skeleton experiment
// found it can't, in the budget tested -- 0/5 completed runs generalized;
// see data/divideconquer_forced_20260918). No RNG needed, same as
// genRecurrenceSkeletonBlock's GcdMod variant: this is one exact,
// deterministic shape, not a family of random variations.
//
// Reserved vars, matching Interpreter.cpp's Call convention: v0=n, v1=lo,
// v2=hi. Fixed locals: v3=mid, v4=i (left pointer, reused as the copy-back
// pointer), v5=j (right pointer), v6=k (aux write pointer). Requires
// numVars >= 7 and, like DivideConquer, only fires when
// GenConfig::recursionCallBias > 0.0 (v1/v2 are otherwise never seeded).
//
// The merge loop's exhausted-side checks (`j >= hi` / `i >= mid`) are
// tested BEFORE ever reading arr[i]/arr[j] on that side -- Mini-C has no
// short-circuit && / ||, so a naive single boolean expression combining
// "i<mid AND (j>=hi OR arr[i]<=arr[j])" would still evaluate arr[j] even
// when j==hi (an out-of-bounds read, since valid indices are [0,n)).
// Structuring it as nested if/else-if instead means each branch only ever
// reads an array slot already known in-range.
StmtPtr genMergeSortSkeletonBlock(Random&, const GenConfig&, int& loopsUsed) {
    const int lo = 1, hi = 2, mid = 3, i = 4, j = 5, k = 6;

    auto takeLeft = [&]() {
        std::vector<StmtPtr> v;
        v.push_back(makeAssignAux(makeVar(k), makeArrayAccess(makeVar(i))));
        v.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));
        return v;
    };
    auto takeRight = [&]() {
        std::vector<StmtPtr> v;
        v.push_back(makeAssignAux(makeVar(k), makeArrayAccess(makeVar(j))));
        v.push_back(makeAssign(j, makeBinOp(ExprKind::Add, makeVar(j), makeIntConst(1))));
        return v;
    };

    // if (arr[i] <= arr[j]) takeLeft else takeRight -- only reached once
    // both i<mid and j<hi are already known true, so both array reads here
    // are safe.
    auto cmpIf =
        makeIf(makeCompare(ExprKind::Le, makeArrayAccess(makeVar(i)), makeArrayAccess(makeVar(j))), takeLeft(), takeRight());
    // if (i >= mid) takeRight else <cmpIf> -- left side exhausted check,
    // before the comparison branch would read arr[i].
    std::vector<StmtPtr> midElse;
    midElse.push_back(std::move(cmpIf));
    auto midIf = makeIf(makeCompare(ExprKind::Ge, makeVar(i), makeVar(mid)), takeRight(), std::move(midElse));
    // if (j >= hi) takeLeft else <midIf> -- right side exhausted check,
    // before anything below would read arr[j].
    std::vector<StmtPtr> outerElse;
    outerElse.push_back(std::move(midIf));
    auto mergeStep = makeIf(makeCompare(ExprKind::Ge, makeVar(j), makeVar(hi)), takeLeft(), std::move(outerElse));

    std::vector<StmtPtr> mergeLoopBody;
    mergeLoopBody.push_back(std::move(mergeStep));
    mergeLoopBody.push_back(makeAssign(k, makeBinOp(ExprKind::Add, makeVar(k), makeIntConst(1))));
    loopsUsed++;
    // while (i<mid OR j<hi) -- both operands are plain scalar comparisons
    // (never an array read), so no short-circuit is needed for safety here.
    auto mergeCond =
        makeBinOp(ExprKind::Add, makeCompare(ExprKind::Lt, makeVar(i), makeVar(mid)), makeCompare(ExprKind::Lt, makeVar(j), makeVar(hi)));
    auto mergeWhile = makeWhile(std::move(mergeCond), std::move(mergeLoopBody));

    std::vector<StmtPtr> copyBody;
    copyBody.push_back(makeAssignArray(makeVar(i), makeAuxAccess(makeVar(i))));
    copyBody.push_back(makeAssign(i, makeBinOp(ExprKind::Add, makeVar(i), makeIntConst(1))));
    loopsUsed++;
    auto copyWhile = makeWhile(makeCompare(ExprKind::Lt, makeVar(i), makeVar(hi)), std::move(copyBody));

    std::vector<StmtPtr> thenBody;
    thenBody.push_back(makeAssign(
        mid, makeBinOp(ExprKind::Add, makeVar(lo),
                        makeBinOp(ExprKind::Div, makeBinOp(ExprKind::Sub, makeVar(hi), makeVar(lo)), makeIntConst(2)))));
    thenBody.push_back(makeCall(makeVar(lo), makeVar(mid)));
    thenBody.push_back(makeCall(makeVar(mid), makeVar(hi)));
    thenBody.push_back(makeAssign(i, makeVar(lo)));
    thenBody.push_back(makeAssign(j, makeVar(mid)));
    thenBody.push_back(makeAssign(k, makeVar(lo)));
    thenBody.push_back(std::move(mergeWhile));
    thenBody.push_back(makeAssign(i, makeVar(lo)));  // reuse i as the copy-back pointer
    thenBody.push_back(std::move(copyWhile));

    return makeIf(makeCompare(ExprKind::Gt, makeBinOp(ExprKind::Sub, makeVar(hi), makeVar(lo)), makeIntConst(1)),
                  std::move(thenBody));
}

// Section 100: generic scalar-loop piece. Inputs X = arr[0], Y = arr[1],
// N = v0; result a, second state b, counter i, temp t. Every part is drawn
// independently, so one draw is factorial, power, average, gcd or a
// primality test only by chance, like any other point of the space:
//   a = {0, 1, X};  [Pair: b = {0, 1, Y}]  [Test: if (X < s) a = c]
//   Counted:   i = {0, 1, 2}; while (i {<, <=} B or i * i {<, <=} B) { body; i = i + 1; }
//   UntilZero: while (b != 0) { body }                       (Pair only)
//   body: Fold  a = a {+, -, *} {i, X, Y, arr[i]}
//         Pair  t = a {+, -, %} b; a = b; b = t;
//         Test  if (X % i == 0) {a = 0 | a = 1 | a = a + 1 | a = a + i}
//   [a = a / {N, X, Y}]  arr[0] = a;
// Section 101 (GenConfig::scalarLoopIndexRange): an arr[i] fold always
// loops i = 0 .. N-1, and the divide is drawn at 1/2 instead of 1/4.
// Asks for 2 vars (Fold, Test), 3 (Pair UntilZero) or 4 (Pair Counted).
StmtPtr genScalarLoopBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    enum { Fold, Pair, Test };
    const int scratch = cfg.numVars - 1;
    int kind = rng.uniformInt(0, 2);
    if (kind == Pair && scratch < 3) kind = Fold;
    bool counted = kind != Pair || (scratch >= 4 && rng.chance(0.5));
    std::vector<int> v = pickDistinctVars(rng, cfg, kind == Pair ? (counted ? 4 : 3) : 2);
    int a = v[0];
    int i = kind == Pair ? (counted ? v[3] : -1) : v[1];
    auto var = [](int x) { return makeVar(x); };
    auto lit = [](int c) { return makeIntConst(c); };
    auto input = [&](int k) { return k == 0 ? makeArrayAccess(lit(0)) : k == 1 ? makeArrayAccess(lit(1)) : var(0); };
    auto one = [](StmtPtr st) {
        std::vector<StmtPtr> out;
        out.push_back(std::move(st));
        return out;
    };

    std::vector<StmtPtr> root;
    int initA = rng.uniformInt(0, 2);
    root.push_back(makeAssign(a, initA < 2 ? lit(initA) : input(0)));
    if (kind == Pair) {
        int initB = rng.uniformInt(0, 2);
        root.push_back(makeAssign(v[1], initB < 2 ? lit(initB) : input(1)));
    }
    if (kind == Test && rng.chance(0.5)) {
        root.push_back(makeIf(makeCompare(ExprKind::Lt, input(0), lit(rng.uniformInt(1, 2))),
                              one(makeAssign(a, lit(rng.uniformInt(0, 1))))));
    }

    std::vector<StmtPtr> body;
    bool indexRange = false;  // section 101: an arr[i] fold loops over the array's own range
    switch (kind) {
        case Fold: {
            static const ExprKind ops[] = {ExprKind::Add, ExprKind::Sub, ExprKind::Mul};
            ExprKind op = ops[rng.uniformInt(0, 2)];
            int term = rng.uniformInt(0, 3);
            indexRange = cfg.scalarLoopIndexRange && term == 3;
            ExprPtr t = term == 0 ? var(i) : term < 3 ? input(term - 1) : makeArrayAccess(var(i));
            body.push_back(makeAssign(a, makeBinOp(op, var(a), std::move(t))));
            break;
        }
        case Pair: {
            static const ExprKind ops[] = {ExprKind::Add, ExprKind::Sub, ExprKind::Mod};
            int b = v[1], t = v[2];
            body.push_back(makeAssign(t, makeBinOp(ops[rng.uniformInt(0, 2)], var(a), var(b))));
            body.push_back(makeAssign(a, var(b)));
            body.push_back(makeAssign(b, var(t)));
            break;
        }
        default: {  // Test
            int update = rng.uniformInt(0, 3);
            ExprPtr value = update < 2 ? lit(update) : makeBinOp(ExprKind::Add, var(a), update == 2 ? lit(1) : var(i));
            body.push_back(makeIf(makeCompare(ExprKind::Eq, makeBinOp(ExprKind::Mod, input(0), var(i)), lit(0)),
                                  one(makeAssign(a, std::move(value)))));
            break;
        }
    }

    loopsUsed++;
    if (indexRange) {  // i = 0; while (i < N)
        root.push_back(makeAssign(i, lit(0)));
        body.push_back(makeAssign(i, makeBinOp(ExprKind::Add, var(i), lit(1))));
        root.push_back(makeWhile(makeCompare(ExprKind::Lt, var(i), input(2)), std::move(body)));
    } else if (counted) {
        root.push_back(makeAssign(i, lit(rng.uniformInt(0, 2))));
        ExprKind cmp = rng.chance(0.5) ? ExprKind::Lt : ExprKind::Le;
        ExprPtr bound = input(rng.uniformInt(0, 2));
        ExprPtr lhs = rng.chance(0.25) ? makeBinOp(ExprKind::Mul, var(i), var(i)) : var(i);
        body.push_back(makeAssign(i, makeBinOp(ExprKind::Add, var(i), lit(1))));
        root.push_back(makeWhile(makeCompare(cmp, std::move(lhs), std::move(bound)), std::move(body)));
    } else {
        root.push_back(makeWhile(makeCompare(ExprKind::Ne, var(v[1]), lit(0)), std::move(body)));
    }
    if (rng.chance(cfg.scalarLoopIndexRange ? 0.5 : 0.25)) {
        static const int divisors[] = {2, 0, 1};  // N, X, Y
        root.push_back(makeAssign(a, makeBinOp(ExprKind::Div, var(a), input(divisors[rng.uniformInt(0, 2)]))));
    }
    root.push_back(makeAssignArray(lit(0), var(a)));
    auto block = makeBlock();
    block->body = std::move(root);
    return block;
}

StmtPtr genIndexIncrementStmt(Random& rng, const GenConfig& cfg) {
    int v = rng.uniformInt(0, cfg.numVars - 1);
    return makeAssign(v, makeBinOp(ExprKind::Add, makeVar(v), makeIntConst(1)));
}

// Moção 2 (Etapa 3) v1: a two-loop block assembled from section 24's
// catalogued primitives under ONE shared binding context (see
// GenConfig::compositeBias). Unlike every named skeleton above -- each a
// hand-written whole algorithm -- this draws each slot independently from a
// small catalog and binds every piece to the SAME context variables, which
// is the binding discipline sections 7/10 showed the free grammar lacks.
// The coherent combination that solves SubstringProblem (pair capture
// arr[0]/arr[arr[0]+1], outer bound n-m, inner bound m, align-match at
// arr[1+i+j] vs arr[n+2+j]) is ONE draw among the family, not the only thing
// this operator can emit.
StmtPtr genCompositeBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    // Binding context: six distinct scratch variables; every piece below
    // references exactly these, never a fresh independent draw. The sixth
    // (`found`) is only used by the scoped-conjunction relation below, but
    // is drawn unconditionally so every composite in the family shares the
    // same binding-context size.
    std::vector<int> v = pickDistinctVars(rng, cfg, 6);
    int vi = v[0], vj = v[1], acc = v[2], capA = v[3], capB = v[4], found = v[5];

    std::vector<StmtPtr> root;
    root.push_back(makeAssign(capA, makeArrayAccess(makeIntConst(0))));  // capA = arr[0]

    // Generalized capture: with probability 0.5 also capB = arr[capA + k]
    // (the "captured scalar at a computed position" primitive -- section 24's
    // captured-scalar only ever read arr[0]/arr[const] before this).
    bool pairCapture = rng.chance(0.5);
    // Coherence prior: the offset that lines the second sequence up right
    // after its length slot (k=1) is the coherent choice; the others exist
    // so the operator is a family, not a single template.
    int k = rng.uniformInt(0, 4);
    if (k > 2) k = 1;
    if (pairCapture) {
        root.push_back(makeAssign(capB, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(capA), makeIntConst(k)))));
    }

    // Outer bound: derived from the captures (aligned n-m+1, plain n, or v0).
    // The aligned choice is the number of valid start positions for a
    // two-sequence scan (i from 0 while i <= n-m, i.e. i < n-m+1) -- the
    // off-by-one n-m version misses the last start and fails at N=1; the n
    // version over-counts and creates false positives. Weighted up when a
    // second sequence was captured; the others keep the operator a family
    // rather than a template.
    ExprPtr outerBound;
    if (pairCapture) {
        int b = rng.uniformInt(0, 3);
        if (b <= 1) {
            outerBound = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Sub, makeVar(capA), makeVar(capB)), makeIntConst(1));
        } else if (b == 2) {
            outerBound = makeVar(capA);
        } else {
            outerBound = makeVar(0);
        }
    } else {
        outerBound = rng.chance(0.5) ? makeVar(capA) : makeVar(0);
    }

    // Inner bound: aligned m, plain n, or a small constant.
    ExprPtr innerBound;
    if (pairCapture) {
        int b = rng.uniformInt(0, 3);
        if (b <= 1) innerBound = makeVar(capB);
        else if (b == 2) innerBound = makeVar(capA);
        else innerBound = makeIntConst(2);
    } else {
        innerBound = rng.chance(0.5) ? makeVar(capA) : makeVar(0);
    }

    // Affine two-sequence index bases. base2 = k + 1 is the offset that lines
    // the second sequence up with the generalized capture (capB = arr[capA+k]
    // means the sequence after it starts at capA + k + 1).
    int base1 = rng.chance(0.75) ? 1 : 2;
    int base2 = k + 1;
    ExprPtr textIdx =
        makeBinOp(ExprKind::Add, makeIntConst(base1), makeBinOp(ExprKind::Add, makeVar(vi), makeVar(vj)));
    ExprPtr patIdx =
        makeBinOp(ExprKind::Add, makeVar(capA), makeBinOp(ExprKind::Add, makeIntConst(base2), makeVar(vj)));

    // Relation piece catalog -- every entry uses vi/vj/acc/capA/capB/found
    // from the context, never fresh variables. Align-match and
    // scoped-conjunction are the two-sequence-coherent relations, so both
    // are only ever drawn when pairCapture.
    int relation = rng.uniformInt(0, 3);
    bool useScopedConjunction = pairCapture && relation == 3;
    bool useSentinelFlag = pairCapture && relation == 0;

    if (useScopedConjunction) {
        // Scoped conjunction ("outer-loop state RESET + inner-loop AND-
        // reduction + post-inner-loop CONSUME"): a per-outer-iteration flag
        // (`acc`) starts true before the inner loop and clears on any
        // mismatch found inside it; only AFTER the inner loop closes is it
        // OR'd into the persistent `found` register. This is a different
        // SCOPE discipline from the other three relations below, which all
        // fire unconditionally once per (i,j) pair inside the inner loop --
        // section 27's isolation test (FINDINGS.md) found the prior catalog
        // (sentinel-flag/count-if/xor) has no piece with this reset-before/
        // consume-after shape, which is exactly what any `exists i, forall
        // j: condition(i,j)` problem needs (substring match, prefix/window
        // equality, row/column validation, ...) -- generic to that whole
        // family, not substring-specific.
        root.push_back(makeAssign(found, makeIntConst(0)));  // found = 0, before the outer loop

        std::vector<StmtPtr> outerBody;
        outerBody.push_back(makeAssign(vj, makeIntConst(0)));
        outerBody.push_back(makeAssign(acc, makeIntConst(1)));  // RESET: acc = 1 per outer iteration
        // Mismatch predicate: which per-(i,j) test clears `acc`. Ne (exact
        // equality required, substring's own condition) vs Le (strict
        // dominance required -- text[i+j] must exceed pattern[j], mismatch
        // when it doesn't) are two different `forall j: cond(i,j)` problems
        // sharing the same reset/AND/consume scope -- kept as a draw, not
        // fixed to Ne, so this relation is a family member like the other
        // three, not a substring-only special case.
        ExprKind mismatchKind = rng.chance(0.5) ? ExprKind::Ne : ExprKind::Le;
        std::vector<StmtPtr> mismatchBody;
        mismatchBody.push_back(makeAssign(acc, makeIntConst(0)));
        std::vector<StmtPtr> innerBody;
        ExprPtr mismatchCond = mismatchKind == ExprKind::Ne
            ? makeCompare(ExprKind::Ne, makeArrayAccess(textIdx->clone()), makeArrayAccess(patIdx->clone()))
            : makeCompare(ExprKind::Le, makeArrayAccess(textIdx->clone()), makeArrayAccess(patIdx->clone()));
        innerBody.push_back(makeIf(std::move(mismatchCond),
            std::move(mismatchBody)));  // AND-reduce: any mismatch clears acc
        appendCountingWhile(outerBody, vj, std::move(innerBound), loopsUsed, [&](std::vector<StmtPtr>& bodyOut) {
            for (auto& stmt : innerBody) bodyOut.push_back(std::move(stmt));
        });
        std::vector<StmtPtr> consumeBody;
        consumeBody.push_back(makeAssign(found, makeIntConst(1)));
        outerBody.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(acc), makeIntConst(1)),
                                   std::move(consumeBody)));  // CONSUME: only after the inner loop closes
        appendCountingWhile(root, vi, std::move(outerBound), loopsUsed, [&](std::vector<StmtPtr>& bodyOut) {
            for (auto& stmt : outerBody) bodyOut.push_back(std::move(stmt));
        });
        root.push_back(makeAssignArray(makeIntConst(0), makeVar(found)));
    } else {
        std::vector<StmtPtr> body;
        if (useSentinelFlag) {
            // Sentinel flag: if (arr[textIdx] == arr[patIdx]) acc = 1
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeIntConst(1)));
            body.push_back(makeIf(
                makeCompare(ExprKind::Eq, makeArrayAccess(textIdx->clone()), makeArrayAccess(patIdx->clone())),
                std::move(then)));
        } else if (relation == 1) {
            // Count-if: if (arr[textIdx] > capB) acc = acc + 1
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeIntConst(1))));
            body.push_back(
                makeIf(makeCompare(ExprKind::Gt, makeArrayAccess(textIdx->clone()), makeVar(capB)), std::move(then)));
        } else {
            // XOR-reduce: acc = acc ^ arr[textIdx]
            body.push_back(makeAssign(acc, makeBinOp(ExprKind::BitXor, makeVar(acc), makeArrayAccess(textIdx->clone()))));
        }

        // Assemble: acc init, outer counting loop (with inner counting loop
        // as its body), then write-back arr[0] = acc.
        root.push_back(makeAssign(acc, makeIntConst(0)));
        std::vector<StmtPtr> outerBody;
        outerBody.push_back(makeAssign(vj, makeIntConst(0)));
        appendCountingWhile(outerBody, vj, std::move(innerBound), loopsUsed, [&](std::vector<StmtPtr>& innerBody) {
            for (auto& stmt : body) innerBody.push_back(std::move(stmt));
        });
        appendCountingWhile(root, vi, std::move(outerBound), loopsUsed, [&](std::vector<StmtPtr>& bodyOut) {
            for (auto& stmt : outerBody) bodyOut.push_back(std::move(stmt));
        });
        root.push_back(makeAssignArray(makeIntConst(0), makeVar(acc)));
    }

    auto block = makeBlock();
    block->body = std::move(root);
    return block;
}

// -- Combinator algebra (GenConfig::combinatorBias, FINDINGS.md section 38) --
//
// Idea B: generalizes genCompositeBlock -- the one mechanism in this log that
// composed a generalizing algorithm without a named skeleton (sections 27,
// 32) -- from one fixed two-loop family into a PRODUCT of independent axes,
// every piece bound to the same six context variables:
//   layout   -- how the input is encoded: whole array, key in arr[0],
//               length-prefixed, or two length-prefixed sequences (the four
//               encodings this repo's problems actually use);
//   loop     -- single counting loop, nested (full), nested triangular
//               (j from i+1), two converging pointers, or (since section
//               40, default since 42; --combinator-v1 drops it) adjacent
//               pairs (elem(i-1), elem(i)) in one pass;
//   relation -- sum, running best, count-if, exists, scoped forall (section
//               32's piece), xor, conditional swap, swap;
//   comparator and write-back follow from the relation.
// Every axis value is a primitive already catalogued in section 24; nothing
// here is new vocabulary. Deliberately absent: pointer chasing (`cycle`) and
// "max over already-computed table cells" (`lis`), the two held-out shapes
// the plenary ruled must not be patched in ad hoc -- they stay out of reach
// by construction and remain the ruler.
namespace combinator {

enum class Layout { Whole, KeyPrefixed, LengthPrefixed, Pair };
enum class Loop { Single, NestedFull, NestedTriangular, Converging, Adjacent };
enum class Relation { Sum, Best, CountIf, Exists, Forall, Xor, SwapIf, Swap, BestStep, CountSegments, Nested, SumStep, Place };

ExprKind drawComparator(Random& rng) {
    static const ExprKind kinds[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Ne, ExprKind::Le, ExprKind::Ge};
    return kinds[rng.uniformInt(0, 5)];
}

// arr[base + x], with `base` a literal or capA + literal.
ExprPtr elemAt(const ExprPtr& base, ExprPtr x) { return makeArrayAccess(makeBinOp(ExprKind::Add, base->clone(), std::move(x))); }

}  // namespace combinator

// Section 82: arg-best update. When `value` beats `best` (keep: Gt for a
// maximum, Lt for a minimum), take it and record `where` in `arg`. Ties:
// 0 = the first wins (strict), 1 = the last wins (non-strict), 2 = the
// smaller `where` wins.
std::vector<StmtPtr> argBestUpdate(const Expr& value, int best, int arg, const Expr& where, ExprKind keep, int tie) {
    auto take = [&] {
        std::vector<StmtPtr> out;
        out.push_back(makeAssign(best, value.clone()));
        out.push_back(makeAssign(arg, where.clone()));
        return out;
    };
    ExprKind cmp = keep;
    if (tie == 1) cmp = keep == ExprKind::Gt ? ExprKind::Ge : ExprKind::Le;
    std::vector<StmtPtr> out;
    if (tie == 2) {
        std::vector<StmtPtr> smaller;
        std::vector<StmtPtr> retake;
        retake.push_back(makeAssign(arg, where.clone()));
        smaller.push_back(makeIf(makeCompare(ExprKind::Lt, where.clone(), makeVar(arg)), std::move(retake)));
        std::vector<StmtPtr> tied;
        tied.push_back(makeIf(makeCompare(ExprKind::Eq, value.clone(), makeVar(best)), std::move(smaller)));
        out.push_back(makeIf(makeCompare(cmp, value.clone(), makeVar(best)), take(), std::move(tied)));
    } else {
        out.push_back(makeIf(makeCompare(cmp, value.clone(), makeVar(best)), take()));
    }
    return out;
}

// Section 55: stateful pieces -- relations whose state carries across
// iterations in a way the section 38/52 relations cannot express. Drawn as
// a quarter of combinator blocks when GenConfig::combinatorState is on, on
// the whole-array or length-prefixed layout.
//   RunBest         adjacent pass; run = cmp(prev, cur) ? run + 1 : 1;
//                   result = the best run.
//   ResetSum        single pass; cur = (cur < 0) ? a : cur + a (Kadane's
//                   restart); result = the best (max or min) cur.
//   EarlierCellsBest  aux[i] = w + best aux[j] over j < i with cmp(a[j], a[i])
//                   (0 when none), w = 1 or a[i]; result = the best aux[i].
//                   The "read several already computed cells" primitive
//                   section 24 predicted DP problems need.
//   PrefixSuffix    aux[i] = best(aux[i-1], a[i]) left to right, then right to
//                   left with a running suffix best r: acc += combine(aux[i], r)
//                   - a[i] or a[i] - combine(...), combine = min or max. The
//                   most problem-shaped piece here (trapping rain water is one
//                   draw of it), parametrized so a post-freeze variant can test
//                   whether it is more than that.
StmtPtr genStatefulBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> v = pickDistinctVars(rng, cfg, 6);
    int vi = v[0], vj = v[1], acc = v[2], cur = v[3], tmp = v[4], capA = v[5];
    auto var = [](int x) { return makeVar(x); };
    auto lit = [](int c) { return makeIntConst(c); };
    auto aux = [](ExprPtr i) { return makeAuxAccess(std::move(i)); };
    auto sub = [](ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Sub, std::move(a), std::move(b)); };
    auto add = [](ExprPtr a, ExprPtr b) { return makeBinOp(ExprKind::Add, std::move(a), std::move(b)); };
    auto one = [](StmtPtr st) {
        std::vector<StmtPtr> out;
        out.push_back(std::move(st));
        return out;
    };
    // if (x keep target) target = x;
    auto keepBest = [&](ExprKind keep, int target, ExprPtr x) {
        ExprPtr copy = x->clone();
        return makeIf(makeCompare(keep, std::move(x), var(target)), one(makeAssign(target, std::move(copy))));
    };
    std::vector<StmtPtr> root;
    int result = acc;  // section 82: the arg variable instead, on an arg-best pass
    // Layout: the whole array (elements arr[0..n)), or length-prefixed
    // (count in arr[0], elements arr[1..count]) -- section 38's two
    // single-sequence encodings. elem(i) = arr[base + i]; aux is indexed by i.
    bool lengthPrefixed = rng.chance(0.5);
    ExprPtr base = lit(lengthPrefixed ? 1 : 0);
    if (lengthPrefixed) root.push_back(makeAssign(capA, makeArrayAccess(lit(0))));
    auto bound = [&] { return var(lengthPrefixed ? capA : 0); };
    auto elem = [&](ExprPtr i) { return makeArrayAccess(add(base->clone(), std::move(i))); };

    bool nestedOk = (cfg.maxLoops - loopsUsed) >= 2;
    // Section 75: FrequencyTable needs three loops (two nested, one pass).
    bool frequencyOk = cfg.combinatorFrequency && (cfg.maxLoops - loopsUsed) >= 3;
    int maxPiece = frequencyOk ? 4 : nestedOk ? 3 : 1;
    // Section 87: DistinctCount is one more equal option (two loops).
    bool distinctOk = cfg.combinatorDistinct && nestedOk;
    int piece = rng.uniformInt(0, maxPiece + (distinctOk ? 1 : 0));
    if (distinctOk && piece == maxPiece + 1) piece = 5;
    ExprKind keep = rng.chance(0.5) ? ExprKind::Gt : ExprKind::Lt;
    switch (piece) {
        case 0: {  // RunBest
            ExprKind cmp = combinator::drawComparator(rng);
            root.push_back(makeAssign(acc, lit(0)));
            root.push_back(makeIf(makeCompare(ExprKind::Lt, lit(0), bound()), one(makeAssign(acc, lit(1)))));
            root.push_back(makeAssign(cur, lit(1)));
            root.push_back(makeAssign(vi, lit(1)));
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& body) {
                body.push_back(makeIf(makeCompare(cmp, elem(sub(var(vi), lit(1))), elem(var(vi))),
                                      one(makeAssign(cur, add(var(cur), lit(1)))), one(makeAssign(cur, lit(1)))));
                body.push_back(keepBest(ExprKind::Gt, acc, var(cur)));
            });
            break;
        }
        case 1: {  // ResetSum
            root.push_back(makeAssign(acc, elem(lit(0))));
            root.push_back(makeAssign(cur, lit(0)));
            root.push_back(makeAssign(vi, lit(0)));
            ExprKind restart = keep == ExprKind::Gt ? ExprKind::Lt : ExprKind::Gt;  // max: restart below 0
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& body) {
                body.push_back(makeIf(makeCompare(restart, var(cur), lit(0)), one(makeAssign(cur, elem(var(vi)))),
                                      one(makeAssign(cur, add(var(cur), elem(var(vi)))))));
                body.push_back(keepBest(keep, acc, var(cur)));
            });
            break;
        }
        case 2: {  // EarlierCellsBest
            ExprKind cmp = combinator::drawComparator(rng);
            bool unitWeight = rng.chance(0.5);
            root.push_back(makeAssign(acc, lit(0)));
            root.push_back(makeAssign(vi, lit(0)));
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& outer) {
                outer.push_back(makeAssign(tmp, lit(0)));
                outer.push_back(makeAssign(vj, lit(0)));
                appendCountingWhile(outer, vj, var(vi), loopsUsed, [&](std::vector<StmtPtr>& inner) {
                    inner.push_back(makeIf(makeCompare(cmp, elem(var(vj)), elem(var(vi))),
                                           one(keepBest(ExprKind::Gt, tmp, aux(var(vj))))));
                });
                outer.push_back(makeAssignAux(var(vi), add(var(tmp), unitWeight ? lit(1) : elem(var(vi)))));
                outer.push_back(keepBest(ExprKind::Gt, acc, aux(var(vi))));
            });
            break;
        }
        case 4: {  // FrequencyTable (section 75)
            static const ExprKind tableCmps[] = {ExprKind::Eq, ExprKind::Lt, ExprKind::Gt};
            ExprKind tableCmp = tableCmps[rng.uniformInt(0, 2)];
            int pass = rng.uniformInt(0, 3);  // max, min, count-if, exists
            ExprKind cmp2 = combinator::drawComparator(rng);
            int thrKind = rng.uniformInt(0, 3);  // 0, 1, 2, n / 2
            root.push_back(makeAssign(vi, lit(0)));
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& outer) {
                outer.push_back(makeAssign(tmp, lit(0)));
                outer.push_back(makeAssign(vj, lit(0)));
                appendCountingWhile(outer, vj, bound(), loopsUsed, [&](std::vector<StmtPtr>& inner) {
                    inner.push_back(makeIf(makeCompare(tableCmp, elem(var(vj)), elem(var(vi))),
                                           one(makeAssign(tmp, add(var(tmp), lit(1))))));
                });
                outer.push_back(makeAssignAux(var(vi), var(tmp)));
            });
            // Section 82: on a max/min pass, return elem(i) or i at the best.
            bool argBest = pass < 2 && cfg.combinatorArgBest && rng.chance(0.5);
            bool argIndex = argBest && rng.chance(0.5);
            int tie = argBest ? rng.uniformInt(0, 2) : 0;
            auto where = [&](ExprPtr i) { return argIndex ? std::move(i) : elem(std::move(i)); };
            root.push_back(makeAssign(acc, lit(0)));
            if (argBest) {
                root.push_back(makeAssign(cur, lit(0)));
                std::vector<StmtPtr> first;
                first.push_back(makeAssign(acc, aux(lit(0))));
                first.push_back(makeAssign(cur, where(lit(0))));
                root.push_back(makeIf(makeCompare(ExprKind::Lt, lit(0), bound()), std::move(first)));
                result = cur;
            } else if (pass < 2) {
                root.push_back(makeIf(makeCompare(ExprKind::Lt, lit(0), bound()), one(makeAssign(acc, aux(lit(0))))));
            }
            root.push_back(makeAssign(vi, lit(0)));
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& body) {
                if (argBest) {
                    for (auto& st : argBestUpdate(*aux(var(vi)), acc, cur, *where(var(vi)), pass == 0 ? ExprKind::Gt : ExprKind::Lt, tie)) {
                        body.push_back(std::move(st));
                    }
                    return;
                }
                if (pass < 2) {
                    body.push_back(keepBest(pass == 0 ? ExprKind::Gt : ExprKind::Lt, acc, aux(var(vi))));
                    return;
                }
                ExprPtr thr = thrKind < 3 ? lit(thrKind) : makeBinOp(ExprKind::Div, bound(), lit(2));
                body.push_back(makeIf(makeCompare(cmp2, aux(var(vi)), std::move(thr)),
                                      one(makeAssign(acc, pass == 2 ? add(var(acc), lit(1)) : lit(1)))));
            });
            break;
        }
        case 5: {  // DistinctCount (section 87)
            // Per element i: t (tmp) = how many equal it, seen (cur) = an
            // equal element earlier. Only first occurrences reach the test.
            ExprKind cmp2 = combinator::drawComparator(rng);
            int thrKind = rng.uniformInt(0, 3);  // 0, 1, 2, n / 2
            bool count = rng.chance(0.5);       // count, or exists
            root.push_back(makeAssign(acc, lit(0)));
            root.push_back(makeAssign(vi, lit(0)));
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& outer) {
                outer.push_back(makeAssign(tmp, lit(0)));
                outer.push_back(makeAssign(cur, lit(0)));
                outer.push_back(makeAssign(vj, lit(0)));
                appendCountingWhile(outer, vj, bound(), loopsUsed, [&](std::vector<StmtPtr>& inner) {
                    std::vector<StmtPtr> equal;
                    equal.push_back(makeAssign(tmp, add(var(tmp), lit(1))));
                    equal.push_back(makeIf(makeCompare(ExprKind::Lt, var(vj), var(vi)), one(makeAssign(cur, lit(1)))));
                    inner.push_back(makeIf(makeCompare(ExprKind::Eq, elem(var(vj)), elem(var(vi))), std::move(equal)));
                });
                ExprPtr thr = thrKind < 3 ? lit(thrKind) : makeBinOp(ExprKind::Div, bound(), lit(2));
                outer.push_back(makeIf(makeCompare(ExprKind::Eq, var(cur), lit(0)),
                                       one(makeIf(makeCompare(cmp2, var(tmp), std::move(thr)),
                                                  one(makeAssign(acc, count ? add(var(acc), lit(1)) : lit(1)))))));
            });
            break;
        }
        default: {  // PrefixSuffix
            bool combineMin = rng.chance(0.5);
            bool waterAbove = rng.chance(0.5);  // combine - a[i] (above) or a[i] - combine (below)
            root.push_back(makeAssign(vi, lit(0)));
            appendCountingWhile(root, vi, bound(), loopsUsed, [&](std::vector<StmtPtr>& body) {
                body.push_back(makeAssign(tmp, elem(var(vi))));
                body.push_back(makeIf(makeCompare(ExprKind::Lt, lit(0), var(vi)),
                                      one(keepBest(keep, tmp, aux(sub(var(vi), lit(1)))))));
                body.push_back(makeAssignAux(var(vi), var(tmp)));
            });
            root.push_back(makeAssign(acc, lit(0)));
            root.push_back(makeAssign(vi, sub(bound(), lit(1))));
            root.push_back(makeIf(makeCompare(ExprKind::Le, lit(0), var(vi)), one(makeAssign(cur, elem(var(vi))))));
            std::vector<StmtPtr> body;
            body.push_back(keepBest(keep, cur, elem(var(vi))));
            body.push_back(makeAssign(tmp, aux(var(vi))));
            body.push_back(keepBest(combineMin ? ExprKind::Lt : ExprKind::Gt, tmp, var(cur)));
            body.push_back(makeAssign(acc, add(var(acc), waterAbove ? sub(var(tmp), elem(var(vi))) : sub(elem(var(vi)), var(tmp)))));
            body.push_back(makeAssign(vi, sub(var(vi), lit(1))));
            loopsUsed++;
            root.push_back(makeWhile(makeCompare(ExprKind::Le, lit(0), var(vi)), std::move(body)));
            break;
        }
    }
    root.push_back(makeAssignArray(lit(0), var(result)));
    auto block = makeBlock();
    block->body = std::move(root);
    return block;
}

// Section 77: pointer following -- the one loop kind no other family has,
// whose next index comes from the data. Node x's pointer is arr[base + x];
// a pointer outside [0, nodes) ends the walk (the null), and the walk is
// capped at v0 steps (pigeonhole: a longer walk must be in a cycle).
//   p = start; steps = 0;
//   while (steps < v0) { if p out of range: break; [visits]; p = arr[base + p];
//                        [two-speed: q advances twice, range-checked; meet?]; steps++ }
// Layout: head-prefixed (base 1, start arr[0]: cycle's encoding) or bare
// (base 0, start 0). Relation: CountSteps (acc = steps), StillInRange (acc =
// 1 if the walk never ended), VisitsKey (acc = 1 if p == k, k in 0..2) or
// TwoSpeed (Floyd: acc = 1 if the two pointers meet).
StmtPtr genFollowBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    std::vector<int> v = pickDistinctVars(rng, cfg, 5);
    int p = v[0], q = v[1], steps = v[2], acc = v[3], nodes = v[4];
    auto var = [](int x) { return makeVar(x); };
    auto lit = [](int c) { return makeIntConst(c); };
    auto one = [](StmtPtr st) {
        std::vector<StmtPtr> out;
        out.push_back(std::move(st));
        return out;
    };
    bool headPrefixed = rng.chance(0.5);
    int base = headPrefixed ? 1 : 0;
    // CountSteps, StillInRange, VisitsKey, TwoSpeed; section 90 adds Trailing, LoopLength.
    int relation = rng.uniformInt(0, cfg.followExtended ? 5 : 3);
    int key = rng.uniformInt(0, 2);
    auto next = [&](int x) { return makeArrayAccess(makeBinOp(ExprKind::Add, lit(base), var(x))); };
    // if (x < 0) break; if (x >= nodes) break;
    auto guard = [&](std::vector<StmtPtr>& body, int x) {
        body.push_back(makeIf(makeCompare(ExprKind::Lt, var(x), lit(0)), one(makeBreak())));
        body.push_back(makeIf(makeCompare(ExprKind::Ge, var(x), var(nodes)), one(makeBreak())));
    };
    std::vector<StmtPtr> root;
    root.push_back(makeAssign(nodes, makeBinOp(ExprKind::Sub, var(0), lit(base))));
    root.push_back(makeAssign(p, headPrefixed ? makeArrayAccess(lit(0)) : lit(0)));
    root.push_back(makeAssign(q, var(p)));
    root.push_back(makeAssign(steps, lit(0)));
    root.push_back(makeAssign(acc, lit(0)));
    if (relation == 4) root.push_back(makeAssign(acc, var(nodes)));  // Trailing: null until a node is seen
    std::vector<StmtPtr> body;
    guard(body, p);
    if (relation == 4) body.push_back(makeAssign(acc, var(p)));
    if (relation == 2) body.push_back(makeIf(makeCompare(ExprKind::Eq, var(p), lit(key)), one(makeAssign(acc, lit(1)))));
    body.push_back(makeAssign(p, next(p)));
    if (relation == 3) {
        for (int k = 0; k < 2; ++k) {
            guard(body, q);
            body.push_back(makeAssign(q, next(q)));
        }
        std::vector<StmtPtr> meet;
        meet.push_back(makeAssign(acc, lit(1)));
        meet.push_back(makeBreak());
        body.push_back(makeIf(makeCompare(ExprKind::Eq, var(p), var(q)), std::move(meet)));
    }
    body.push_back(makeAssign(steps, makeBinOp(ExprKind::Add, var(steps), lit(1))));
    loopsUsed++;
    root.push_back(makeWhile(makeCompare(ExprKind::Lt, var(steps), var(0)), std::move(body)));
    if (relation == 0) root.push_back(makeAssign(acc, var(steps)));
    // `onCycle` runs only if the walk never ended (p still in range).
    auto ifStillInRange = [&](std::vector<StmtPtr> onCycle) {
        root.push_back(makeIf(makeCompare(ExprKind::Le, lit(0), var(p)),
                              one(makeIf(makeCompare(ExprKind::Lt, var(p), var(nodes)), std::move(onCycle)))));
    };
    if (relation == 1) ifStillInRange(one(makeAssign(acc, lit(1))));
    if (relation == 4) ifStillInRange(one(makeAssign(acc, var(nodes))));
    if (relation == 5) {
        // q = p; steps = 0; while (steps < v0) { p = next(p); acc++; if (p == q) break; steps++ }
        std::vector<StmtPtr> loop;
        loop.push_back(makeAssign(p, next(p)));
        loop.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, var(acc), lit(1))));
        loop.push_back(makeIf(makeCompare(ExprKind::Eq, var(p), var(q)), one(makeBreak())));
        loop.push_back(makeAssign(steps, makeBinOp(ExprKind::Add, var(steps), lit(1))));
        std::vector<StmtPtr> onCycle;
        onCycle.push_back(makeAssign(q, var(p)));
        onCycle.push_back(makeAssign(steps, lit(0)));
        loopsUsed++;
        onCycle.push_back(makeWhile(makeCompare(ExprKind::Lt, var(steps), var(0)), std::move(loop)));
        ifStillInRange(std::move(onCycle));
    }
    root.push_back(makeAssignArray(lit(0), var(acc)));
    auto block = makeBlock();
    block->body = std::move(root);
    return block;
}

StmtPtr genCombinatorBlock(Random& rng, const GenConfig& cfg, int& loopsUsed) {
    using namespace combinator;
    if (cfg.combinatorState && rng.uniformInt(0, 3) == 0) return genStatefulBlock(rng, cfg, loopsUsed);
    if (cfg.combinatorFollow && (!cfg.followPointerInputsOnly || cfg.pointerInput) && rng.uniformInt(0, 7) == 0) {
        return genFollowBlock(rng, cfg, loopsUsed);
    }
    std::vector<int> v = pickDistinctVars(rng, cfg, 6);
    int vi = v[0], vj = v[1], acc = v[2], capA = v[3], capB = v[4], found = v[5];

    bool nestedOk = (cfg.maxLoops - loopsUsed) >= 2;
    // Pair is always a nested window, so it needs two free loop slots.
    Layout layout = static_cast<Layout>(rng.uniformInt(0, nestedOk ? 3 : 2));
    std::vector<StmtPtr> root;

    // Layout: element base, element count, and the key operand (if any).
    ExprPtr base, bound, base2, bound2, key;
    switch (layout) {
        case Layout::Whole:
            base = makeIntConst(0);
            bound = makeVar(0);
            key = makeIntConst(rng.uniformInt(0, cfg.maxConst));
            break;
        case Layout::KeyPrefixed:
            root.push_back(makeAssign(capA, makeArrayAccess(makeIntConst(0))));
            base = makeIntConst(0);  // haystack is arr[1..n-1]; the loop starts at 1
            bound = makeVar(0);
            key = makeVar(capA);
            break;
        case Layout::LengthPrefixed:
            root.push_back(makeAssign(capA, makeArrayAccess(makeIntConst(0))));
            base = makeIntConst(1);
            bound = makeVar(capA);
            key = makeIntConst(rng.uniformInt(0, cfg.maxConst));
            break;
        case Layout::Pair:
            root.push_back(makeAssign(capA, makeArrayAccess(makeIntConst(0))));
            root.push_back(makeAssign(capB, makeArrayAccess(makeBinOp(ExprKind::Add, makeVar(capA), makeIntConst(1)))));
            base = makeIntConst(1);
            bound = makeVar(capA);
            base2 = makeBinOp(ExprKind::Add, makeVar(capA), makeIntConst(2));
            bound2 = makeVar(capB);
            break;
    }
    int loopStart = layout == Layout::KeyPrefixed ? 1 : 0;

    // Loop: a pair layout is a sliding window (outer start i, inner offset j);
    // every other layout draws freely. Single needs one free loop slot, the
    // rest two.
    Loop loop;
    if (layout == Layout::Pair) {
        loop = Loop::NestedFull;
    } else {
        int maxLoop = nestedOk ? 3 : 0;
        // Adjacent (section 40) is one more option unless cfg asks for the
        // frozen v1 family (--combinator-v1), which stays byte-identical.
        int options = maxLoop + 1 + (cfg.combinatorAdjacent ? 1 : 0);
        int pick = rng.uniformInt(0, options - 1);
        loop = pick <= maxLoop ? static_cast<Loop>(pick) : Loop::Adjacent;
    }

    // Relation, restricted to what makes sense for the loop's operand arity:
    // a single loop sees one element; nested and converging loops see a pair.
    static const Relation singleRel[] = {Relation::Sum, Relation::Best, Relation::CountIf, Relation::Exists, Relation::Xor};
    static const Relation pairRel[] = {Relation::CountIf, Relation::Exists, Relation::Forall, Relation::SwapIf, Relation::Swap};
    Relation rel = loop == Loop::Single ? singleRel[rng.uniformInt(0, 4)] : pairRel[rng.uniformInt(0, 4)];
    if (layout == Layout::Pair && (rel == Relation::SwapIf || rel == Relation::Swap)) rel = Relation::Forall;
    // Section 52: on an adjacent pass, the two step relations join the five
    // pair relations as equals (2 of 7). Drawn only with the flag on, so the
    // RNG stream is unchanged without it.
    if (cfg.combinatorStep && loop == Loop::Adjacent && cfg.combinatorOrthogonal) {
        // Section 80: SumStep joins as a third step relation (3 of 8).
        static const Relation stepRel[] = {Relation::BestStep, Relation::CountSegments, Relation::SumStep};
        int r = rng.uniformInt(0, 7);
        if (r >= 5) rel = stepRel[r - 5];
    } else if (cfg.combinatorStep && loop == Loop::Adjacent && rng.uniformInt(0, 6) >= 5) {
        rel = rng.chance(0.5) ? Relation::BestStep : Relation::CountSegments;
    }
    // Section 71: on a nested loop over one sequence, the nested aggregate
    // joins the five pair relations (2 of 7). Drawn only with the flag on.
    if (cfg.combinatorNested && (loop == Loop::NestedFull || loop == Loop::NestedTriangular) &&
        (layout == Layout::Whole || layout == Layout::LengthPrefixed) && rng.uniformInt(0, 6) >= 5) {
        rel = Relation::Nested;
    }
    // Section 102b: placement as one more pair relation, always triangular.
    if (cfg.nestedPlaceOption && (loop == Loop::NestedFull || loop == Loop::NestedTriangular) &&
        (layout == Layout::Whole || layout == Layout::LengthPrefixed) && rng.uniformInt(0, 5) == 0) {
        rel = Relation::Place;
        loop = Loop::NestedTriangular;
    }
    ExprKind cmp = drawComparator(rng);

    // Operands for this loop shape.
    ExprPtr a, b, posA, posB;
    if (layout == Layout::Pair) {
        a = elemAt(base, makeBinOp(ExprKind::Add, makeVar(vi), makeVar(vj)));  // text[i + j]
        b = elemAt(base2, makeVar(vj));                                        // pattern[j]
    } else {
        posA = makeBinOp(ExprKind::Add, base->clone(), makeVar(vi));
        if (loop == Loop::Adjacent) {  // operands elem(i-1), elem(i)
            posA = makeBinOp(ExprKind::Sub, makeBinOp(ExprKind::Add, base->clone(), makeVar(vi)), makeIntConst(1));
        }
        a = makeArrayAccess(posA->clone());
        if (loop == Loop::Adjacent) {
            posB = makeBinOp(ExprKind::Add, base->clone(), makeVar(vi));
            b = makeArrayAccess(posB->clone());
        } else if (loop != Loop::Single) {
            posB = makeBinOp(ExprKind::Add, base->clone(), makeVar(vj));
            b = makeArrayAccess(posB->clone());
        }
    }

    // The relation's statement at the innermost point, its accumulator
    // init, and whether the result is written back to arr[0].
    std::vector<StmtPtr> inner;
    std::vector<StmtPtr> perOuterReset, perOuterConsume;  // Forall on nested loops only
    bool writeBack = true;
    int accInit = 0;
    int resultVar = acc;
    ExprPtr argInit;        // section 82: initial value of the arg variable
    bool absStep = false;   // section 71
    bool stepFlip = false;  // section 80
    ExprPtr rhs = b ? b->clone() : key->clone();
    switch (rel) {
        case Relation::Sum:
            inner.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), a->clone())));
            break;
        case Relation::Xor:
            inner.push_back(makeAssign(acc, makeBinOp(ExprKind::BitXor, makeVar(acc), a->clone())));
            break;
        case Relation::Best: {
            ExprKind keep = rng.chance(0.5) ? ExprKind::Lt : ExprKind::Gt;
            // Section 82: arg-best returns the index i of the best (its
            // value is already the element), in `found`.
            if (cfg.combinatorArgBest && rng.chance(0.5)) {
                int tie = rng.uniformInt(0, 2);
                for (auto& st : argBestUpdate(*a, acc, found, *makeVar(vi), keep, tie)) inner.push_back(std::move(st));
                resultVar = found;
                argInit = makeIntConst(loopStart);
            } else {
                std::vector<StmtPtr> then;
                then.push_back(makeAssign(acc, a->clone()));
                inner.push_back(makeIf(makeCompare(keep, a->clone(), makeVar(acc)), std::move(then)));
            }
            accInit = -1;  // sentinel: initialized from the first element below
            break;
        }
        case Relation::CountIf: {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeIntConst(1))));
            inner.push_back(makeIf(makeCompare(cmp, a->clone(), std::move(rhs)), std::move(then)));
            break;
        }
        case Relation::Exists: {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeIntConst(1)));
            inner.push_back(makeIf(makeCompare(cmp, a->clone(), std::move(rhs)), std::move(then)));
            break;
        }
        case Relation::Forall: {
            // Mismatch clears the flag. On a converging loop the scope is the
            // whole walk (flag starts 1, written at the end); on a nested loop
            // it is section 32's reset-per-outer / consume-after-inner shape.
            std::vector<StmtPtr> clear;
            clear.push_back(makeAssign(acc, makeIntConst(0)));
            inner.push_back(makeIf(makeCompare(cmp, a->clone(), std::move(rhs)), std::move(clear)));
            if (loop == Loop::Converging || loop == Loop::Adjacent) {  // one flat scope: the whole walk
                accInit = 1;
            } else {
                perOuterReset.push_back(makeAssign(acc, makeIntConst(1)));
                std::vector<StmtPtr> consume;
                consume.push_back(makeAssign(found, makeIntConst(1)));
                perOuterConsume.push_back(makeIf(makeCompare(ExprKind::Eq, makeVar(acc), makeIntConst(1)), std::move(consume)));
                resultVar = found;
            }
            break;
        }
        case Relation::SwapIf: {
            std::vector<StmtPtr> then;
            then.push_back(makeSwap(posA->clone(), posB->clone()));
            inner.push_back(makeIf(makeCompare(cmp, a->clone(), std::move(rhs)), std::move(then)));
            writeBack = false;
            break;
        }
        case Relation::Swap:
            inner.push_back(makeSwap(posA->clone(), posB->clone()));
            writeBack = false;
            break;
        case Relation::SumStep:
        case Relation::BestStep: {
            // acc = min or max (BestStep) or the sum (SumStep, section 80) of
            // elem(i) - elem(i-1), or of elem(i-1) - elem(i) when the
            // direction axis flips it; `found` holds the step.
            stepFlip = cfg.combinatorOrthogonal && rng.chance(0.5);
            inner.push_back(makeAssign(found, stepFlip ? makeBinOp(ExprKind::Sub, a->clone(), b->clone())
                                                       : makeBinOp(ExprKind::Sub, b->clone(), a->clone())));
            // Section 71: |step|, one time in three with the flag on (section
            // 80 extends it to SumStep).
            absStep = (rel == Relation::SumStep ? cfg.combinatorOrthogonal : cfg.combinatorNested) && rng.uniformInt(0, 2) == 0;
            if (absStep) {
                std::vector<StmtPtr> negate;
                negate.push_back(makeAssign(found, makeBinOp(ExprKind::Sub, makeIntConst(0), makeVar(found))));
                inner.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(found), makeIntConst(0)), std::move(negate)));
            }
            if (rel == Relation::SumStep) {
                inner.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeVar(found))));
                break;
            }
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeVar(found)));
            inner.push_back(makeIf(makeCompare(rng.chance(0.5) ? ExprKind::Lt : ExprKind::Gt, makeVar(found), makeVar(acc)),
                                   std::move(then)));
            accInit = -2;  // sentinel: first step if there is one, else 0 (below)
            break;
        }
        case Relation::CountSegments: {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeIntConst(1))));
            inner.push_back(makeIf(makeCompare(cmp, a->clone(), std::move(rhs)), std::move(then)));
            accInit = 1;
            break;
        }
        case Relation::Place:
        case Relation::Nested: {
            if (rel == Relation::Place || (cfg.nestedPlace && rng.uniformInt(0, 2) == 0)) {
                // Section 102: t = i; if (elem(j) CMP elem(t)) t = j; then
                // swap(elem(i), elem(t)) once per outer iteration.
                static const ExprKind placeCmp[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Le, ExprKind::Ge};
                ExprKind pc = placeCmp[rng.uniformInt(0, 3)];
                perOuterReset.push_back(makeAssign(found, makeVar(vi)));
                std::vector<StmtPtr> then;
                then.push_back(makeAssign(found, makeVar(vj)));
                inner.push_back(makeIf(makeCompare(pc, b->clone(), elemAt(base, makeVar(found))), std::move(then)));
                perOuterConsume.push_back(makeSwap(posA->clone(), makeBinOp(ExprKind::Add, base->clone(), makeVar(found))));
                writeBack = false;
                break;
            }
            // t (held in `found`) aggregates elem(j) against elem(i) over the
            // inner loop, reset per outer element; acc aggregates the t_i.
            int innerKind = rng.uniformInt(0, 2);  // CountCmp, ForallCmp, BestDiff
            int outerKind = rng.uniformInt(0, 2);  // Best, CountIf, ExistsIf
            ExprKind cmp2 = drawComparator(rng);
            int thrKind = rng.uniformInt(0, 2);  // 0, 1, bound / 2
            const int t = found;
            perOuterReset.push_back(makeAssign(t, makeIntConst(innerKind == 1 ? 1 : 0)));
            std::vector<StmtPtr> then;
            if (innerKind == 0) {
                then.push_back(makeAssign(t, makeBinOp(ExprKind::Add, makeVar(t), makeIntConst(1))));
                inner.push_back(makeIf(makeCompare(cmp, b->clone(), a->clone()), std::move(then)));
            } else if (innerKind == 1) {
                then.push_back(makeAssign(t, makeIntConst(0)));
                inner.push_back(makeIf(makeCompare(cmp, b->clone(), a->clone()), std::move(then)));
            } else {
                // Section 80: elem(i) - elem(j) instead, half the time with the flag on.
                bool flip = cfg.combinatorOrthogonal && rng.chance(0.5);
                auto diff = [&] { return flip ? makeBinOp(ExprKind::Sub, a->clone(), b->clone()) : makeBinOp(ExprKind::Sub, b->clone(), a->clone()); };
                then.push_back(makeAssign(t, diff()));
                inner.push_back(makeIf(makeCompare(ExprKind::Gt, diff(), makeVar(t)), std::move(then)));
            }
            ExprPtr thr = thrKind == 0 ? makeIntConst(0)
                          : thrKind == 1 ? makeIntConst(1)
                                         : makeBinOp(ExprKind::Div, bound->clone(), makeIntConst(2));
            if (cfg.nestedCanonicalTests) {
                // Section 73: one of six distinct tests on t >= 0.
                static const ExprKind ops[] = {ExprKind::Eq, ExprKind::Gt, ExprKind::Eq, ExprKind::Gt, ExprKind::Gt, ExprKind::Le};
                int test = rng.uniformInt(0, 5);
                cmp2 = ops[test];
                thr = test < 2 ? makeIntConst(0)
                      : test < 4 ? makeIntConst(1)
                                 : makeBinOp(ExprKind::Div, bound->clone(), makeIntConst(2));
            }
            std::vector<StmtPtr> consume;
            if (outerKind == 0 && cfg.combinatorArgBest && rng.chance(0.5)) {
                // Section 82: the element (or index) with the best t, in capB.
                bool index = rng.chance(0.5);
                int tie = rng.uniformInt(0, 2);
                ExprPtr where = index ? makeVar(vi) : a->clone();
                for (auto& st : argBestUpdate(*makeVar(t), acc, capB, *where, ExprKind::Gt, tie)) perOuterConsume.push_back(std::move(st));
                resultVar = capB;
                argInit = makeIntConst(0);
            } else if (outerKind == 0) {
                consume.push_back(makeAssign(acc, makeVar(t)));
                perOuterConsume.push_back(makeIf(makeCompare(ExprKind::Gt, makeVar(t), makeVar(acc)), std::move(consume)));
            } else {
                consume.push_back(makeAssign(acc, outerKind == 1 ? makeBinOp(ExprKind::Add, makeVar(acc), makeIntConst(1))
                                                                 : makeIntConst(1)));
                perOuterConsume.push_back(makeIf(makeCompare(cmp2, makeVar(t), std::move(thr)), std::move(consume)));
            }
            break;
        }
    }

    if (writeBack) {
        if (accInit == -1) {
            root.push_back(makeAssign(acc, makeArrayAccess(makeBinOp(ExprKind::Add, base->clone(), makeIntConst(loopStart)))));
        } else if (accInit == -2) {
            // acc = 0; if (start + 1 < bound) acc = elem(start + 1) - elem(start);
            root.push_back(makeAssign(acc, makeIntConst(0)));
            std::vector<StmtPtr> first;
            ExprPtr later = makeArrayAccess(makeBinOp(ExprKind::Add, base->clone(), makeIntConst(loopStart + 1)));
            ExprPtr earlier = makeArrayAccess(makeBinOp(ExprKind::Add, base->clone(), makeIntConst(loopStart)));
            first.push_back(makeAssign(acc, stepFlip ? makeBinOp(ExprKind::Sub, std::move(earlier), std::move(later))
                                                     : makeBinOp(ExprKind::Sub, std::move(later), std::move(earlier))));
            root.push_back(makeIf(makeCompare(ExprKind::Lt, makeIntConst(loopStart + 1), bound->clone()), std::move(first)));
            if (absStep) {
                std::vector<StmtPtr> negate;
                negate.push_back(makeAssign(acc, makeBinOp(ExprKind::Sub, makeIntConst(0), makeVar(acc))));
                root.push_back(makeIf(makeCompare(ExprKind::Lt, makeVar(acc), makeIntConst(0)), std::move(negate)));
            }
        } else {
            root.push_back(makeAssign(acc, makeIntConst(accInit)));
        }
        if (resultVar == found && !argInit) root.push_back(makeAssign(found, makeIntConst(0)));
        if (argInit) root.push_back(makeAssign(resultVar, std::move(argInit)));
    }

    // Loop assembly.
    switch (loop) {
        case Loop::Single:
            root.push_back(makeAssign(vi, makeIntConst(loopStart)));
            appendCountingWhile(root, vi, bound->clone(), loopsUsed, [&](std::vector<StmtPtr>& body) {
                for (auto& s : inner) body.push_back(std::move(s));
            });
            break;
        case Loop::NestedFull:
        case Loop::NestedTriangular: {
            ExprPtr outerBound = bound->clone();
            ExprPtr innerBound = bound->clone();
            if (layout == Layout::Pair) {
                // window starts: n - m + 1 (the aligned bound section 27 fixed)
                outerBound = makeBinOp(ExprKind::Add, makeBinOp(ExprKind::Sub, makeVar(capA), makeVar(capB)), makeIntConst(1));
                innerBound = bound2->clone();
            }
            std::vector<StmtPtr> outerBody;
            ExprPtr jStart = loop == Loop::NestedTriangular ? makeBinOp(ExprKind::Add, makeVar(vi), makeIntConst(1))
                                                             : makeIntConst(loopStart);
            outerBody.push_back(makeAssign(vj, std::move(jStart)));
            for (auto& s : perOuterReset) outerBody.push_back(std::move(s));
            appendCountingWhile(outerBody, vj, std::move(innerBound), loopsUsed, [&](std::vector<StmtPtr>& body) {
                for (auto& s : inner) body.push_back(std::move(s));
            });
            for (auto& s : perOuterConsume) outerBody.push_back(std::move(s));
            root.push_back(makeAssign(vi, makeIntConst(loopStart)));
            appendCountingWhile(root, vi, std::move(outerBound), loopsUsed, [&](std::vector<StmtPtr>& body) {
                for (auto& s : outerBody) body.push_back(std::move(s));
            });
            break;
        }
        case Loop::Adjacent:
            // i from start + 1: every adjacent pair (elem(i-1), elem(i)).
            root.push_back(makeAssign(vi, makeIntConst(loopStart + 1)));
            appendCountingWhile(root, vi, bound->clone(), loopsUsed, [&](std::vector<StmtPtr>& body) {
                for (auto& s : inner) body.push_back(std::move(s));
            });
            break;
        case Loop::Converging: {
            // lo = start, hi = bound - 1; while (lo < hi) { rel; lo++; hi--; }
            root.push_back(makeAssign(vi, makeIntConst(loopStart)));
            root.push_back(makeAssign(vj, makeBinOp(ExprKind::Sub, bound->clone(), makeIntConst(1))));
            std::vector<StmtPtr> body;
            for (auto& s : inner) body.push_back(std::move(s));
            body.push_back(makeAssign(vi, makeBinOp(ExprKind::Add, makeVar(vi), makeIntConst(1))));
            body.push_back(makeAssign(vj, makeBinOp(ExprKind::Sub, makeVar(vj), makeIntConst(1))));
            loopsUsed++;
            root.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(vi), makeVar(vj)), std::move(body)));
            break;
        }
    }

    if (writeBack) root.push_back(makeAssignArray(makeIntConst(0), makeVar(resultVar)));

    auto block = makeBlock();
    block->body = std::move(root);
    return block;
}

// -- Role-typed variables (GenConfig::roleTypedVars, FINDINGS.md section 36) --
//
// Every variable gets one fixed role by its index, the same for the whole
// program and across later mutations (growProgram inserts statements long
// after the first draw, so a per-program assignment could not be kept
// consistent): v0 is the bound n (the interpreter seeds it with the input
// length; never assigned here), odd scratch variables are indices, even
// scratch variables are values. Array positions only ever come from index
// expressions and scalar comparisons never mix the two kinds -- the binding
// discipline section 27 found causal for the composite family, applied to
// the free grammar. Section 35 measured ~90% of generated candidates
// duplicating an already-seen behavior, most of them identical crashes;
// incoherent index use (an accumulator as arr's subscript) is the suspected
// source.
bool isIndexVar(int v) { return v > 0 && v % 2 == 1; }

int pickIndexVar(Random& rng, const GenConfig& cfg) {
    int count = cfg.numVars / 2;  // v1, v3, ... below numVars
    if (count == 0) return 0;
    return 2 * rng.uniformInt(0, count - 1) + 1;
}

// Falls back to an index var when numVars leaves no even scratch slot.
int pickValueVar(Random& rng, const GenConfig& cfg) {
    int count = (cfg.numVars - 1) / 2;  // v2, v4, ... below numVars
    if (count == 0) return pickIndexVar(rng, cfg);
    return 2 * rng.uniformInt(1, count);
}

// Also covers the mirrored subscript `n - index - literal` (reverse,
// palindrome-style scans) and `index + index` (two-sequence offsets): the
// first version lacked both, and section 36's own novelty measurement caught
// it (reverse's new-behavior rate halved).
ExprPtr genIndexExpr(Random& rng, const GenConfig& cfg) {
    int choice = rng.uniformInt(0, 11);
    if (choice < 5) return makeVar(pickIndexVar(rng, cfg));
    if (choice < 7) return makeIntConst(rng.uniformInt(0, cfg.maxConst));
    if (choice < 9) {
        ExprKind k = rng.chance(0.5) ? ExprKind::Add : ExprKind::Sub;
        return makeBinOp(k, makeVar(pickIndexVar(rng, cfg)), makeIntConst(rng.uniformInt(1, std::max(1, cfg.maxConst))));
    }
    if (choice == 9) return makeBinOp(ExprKind::Sub, makeVar(0), makeIntConst(rng.uniformInt(1, std::max(1, cfg.maxConst))));
    if (choice == 10) {
        ExprPtr mirrored = makeBinOp(ExprKind::Sub, makeVar(0), makeVar(pickIndexVar(rng, cfg)));
        return makeBinOp(ExprKind::Sub, std::move(mirrored), makeIntConst(rng.uniformInt(0, 1)));
    }
    ExprPtr first = makeVar(pickIndexVar(rng, cfg));
    return makeBinOp(ExprKind::Add, std::move(first), makeVar(pickIndexVar(rng, cfg)));
}

// What an index variable may be assigned: a start (0/1), a step (+/- 1), the
// bound, or another index.
ExprPtr genIndexUpdateExpr(Random& rng, const GenConfig& cfg, int target) {
    int choice = rng.uniformInt(0, 5);
    switch (choice) {
        case 0: return makeIntConst(rng.uniformInt(0, 1));
        case 1: return makeBinOp(ExprKind::Add, makeVar(target), makeIntConst(1));
        case 2: return makeBinOp(ExprKind::Sub, makeVar(target), makeIntConst(1));
        case 3: return makeVar(0);
        default: return genIndexExpr(rng, cfg);
    }
}

ExprPtr genTypedValueExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    if (depthBudget <= 0 || rng.chance(0.35)) {
        int leaf = rng.uniformInt(0, 2);
        if (leaf == 0) return makeIntConst(rng.uniformInt(0, cfg.maxConst));
        if (leaf == 1) return makeVar(pickValueVar(rng, cfg));
        return makeArrayAccess(genIndexExpr(rng, cfg));
    }
    if (rng.chance(0.08)) {  // same always-on arithmetic/bitwise substitution as genValueExpr
        static const ExprKind extKinds[] = {ExprKind::Mul,    ExprKind::Div,    ExprKind::Mod,   ExprKind::BitAnd,
                                             ExprKind::BitOr, ExprKind::BitXor, ExprKind::Shl,   ExprKind::Shr};
        ExprKind k = extKinds[rng.uniformInt(0, 7)];
        ExprPtr lhs = genTypedValueExpr(rng, depthBudget - 1, cfg);
        return makeBinOp(k, std::move(lhs), genTypedValueExpr(rng, depthBudget - 1, cfg));
    }
    int choice = rng.uniformInt(0, 3);
    switch (choice) {
        case 0: return makeArrayAccess(genIndexExpr(rng, cfg));
        case 1:
        case 2: {
            ExprPtr lhs = genTypedValueExpr(rng, depthBudget - 1, cfg);
            return makeBinOp(choice == 1 ? ExprKind::Add : ExprKind::Sub, std::move(lhs),
                             genTypedValueExpr(rng, depthBudget - 1, cfg));
        }
        default: return makeVar(pickValueVar(rng, cfg));
    }
}

// Either an index-side test (index against the bound or another index) or a
// value-side test (value/element against value/element), never mixed.
ExprPtr genTypedCondExpr(Random& rng, const GenConfig& cfg) {
    ExprKind kinds[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    ExprKind k = kinds[rng.uniformInt(0, 5)];
    if (rng.chance(0.5)) {
        ExprPtr lhs = makeVar(pickIndexVar(rng, cfg));
        return makeCompare(k, std::move(lhs), rng.chance(0.6) ? makeVar(0) : genIndexExpr(rng, cfg));
    }
    ExprPtr lhs = genTypedValueExpr(rng, cfg.maxExprDepth, cfg);
    return makeCompare(k, std::move(lhs), genTypedValueExpr(rng, cfg.maxExprDepth, cfg));
}

// The condition used by genStmt's generic if/while and growProgram's
// condition mutation: role-typed when enabled, otherwise the ordinary draw.
ExprPtr genStmtCondExpr(Random& rng, const GenConfig& cfg) {
    if (!cfg.roleTypedVars) return genCondExpr(rng, cfg.maxExprDepth, cfg);
    gGenConds++;
    ExprPtr c = genTypedCondExpr(rng, cfg);
    prunePart(cfg, ConstantCondition, c, [&] { return genTypedCondExpr(rng, cfg); },
              [&] { return constantConditionValue(*c).has_value(); });
    return c;
}

// Generic (non-skeleton) statement builders for genStmt's ordinary paths,
// each wired to its GenConfig::pruneGeneration rule. Every one builds the
// statement exactly as genStmt used to inline, then checks it post hoc (see
// prunePart).
StmtPtr genPlainAssign(Random& rng, const GenConfig& cfg) {
    gGenAssigns++;
    if (cfg.roleTypedVars) {
        // Never assigns v0 (the bound); index vars get index updates only.
        int target = rng.chance(0.5) ? pickIndexVar(rng, cfg) : pickValueVar(rng, cfg);
        auto gen = [&]() -> ExprPtr {
            return isIndexVar(target) ? genIndexUpdateExpr(rng, cfg, target) : genTypedValueExpr(rng, cfg.maxExprDepth, cfg);
        };
        StmtPtr s = makeAssign(target, gen());
        prunePart(cfg, SelfAssign, s->valueExpr, gen,
                  [&] { return s->valueExpr->kind == ExprKind::Var && s->valueExpr->varIndex == s->varIndex; });
        return s;
    }
    StmtPtr s = makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
    prunePart(cfg, SelfAssign, s->valueExpr, [&] { return genValueExpr(rng, cfg.maxExprDepth, cfg); },
              [&] { return s->valueExpr->kind == ExprKind::Var && s->valueExpr->varIndex == s->varIndex; });
    return s;
}

// arr[e] = arr[e] (or aux[e] = aux[e]): the value read is the same cell of
// the same array being written.
StmtPtr genPlainArrayAssign(Random& rng, const GenConfig& cfg, bool aux) {
    gGenArrayAssigns++;
    ExprKind readKind = aux ? ExprKind::AuxAccess : ExprKind::ArrayAccess;
    if (cfg.roleTypedVars) {
        ExprPtr index = genIndexExpr(rng, cfg);
        StmtPtr s = aux ? makeAssignAux(std::move(index), genTypedValueExpr(rng, cfg.maxExprDepth, cfg))
                        : makeAssignArray(std::move(index), genTypedValueExpr(rng, cfg.maxExprDepth, cfg));
        prunePart(cfg, ArraySelfAssign, s->valueExpr, [&] { return genTypedValueExpr(rng, cfg.maxExprDepth, cfg); }, [&] {
            return s->valueExpr->kind == readKind && exprEqual(s->valueExpr->lhs.get(), s->indexExpr.get());
        });
        return s;
    }
    StmtPtr s = aux ? makeAssignAux(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg))
                    : makeAssignArray(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
    prunePart(cfg, ArraySelfAssign, s->valueExpr, [&] { return genValueExpr(rng, cfg.maxExprDepth, cfg); }, [&] {
        return s->valueExpr->kind == readKind && exprEqual(s->valueExpr->lhs.get(), s->indexExpr.get());
    });
    return s;
}

StmtPtr genPlainSwap(Random& rng, const GenConfig& cfg) {
    gGenSwaps++;
    if (cfg.roleTypedVars) {
        ExprPtr first = genIndexExpr(rng, cfg);
        StmtPtr s = makeSwap(std::move(first), genIndexExpr(rng, cfg));
        prunePart(cfg, SelfSwap, s->indexExpr2, [&] { return genIndexExpr(rng, cfg); },
                  [&] { return exprEqual(s->indexExpr.get(), s->indexExpr2.get()); });
        return s;
    }
    StmtPtr s = makeSwap(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
    prunePart(cfg, SelfSwap, s->indexExpr2, [&] { return genValueExpr(rng, cfg.maxExprDepth, cfg); },
              [&] { return exprEqual(s->indexExpr.get(), s->indexExpr2.get()); });
    return s;
}

StmtPtr genStmt(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, bool insideIf = false,
                bool insideWhile = false) {
    // Control-flow extension (see GenConfig::controlFlowBias): only valid,
    // and only ever emitted, directly inside a while loop's reach (its own
    // body or an if nested inside it) -- checked first, before anything
    // else gets a chance to consume this statement slot, same early-out
    // style as the small always-on arithmetic-op substitution below.
    if (insideWhile && cfg.controlFlowBias > 0.0 && rng.chance(cfg.controlFlowBias)) {
        return rng.chance(0.7) ? makeContinue() : makeBreak();
    }
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
    // The scalar recurrences, divisor loops and repeated multiplies are no
    // longer hand-written skeletons: section 100's generic scalar-loop
    // piece (genScalarLoopBlock) reaches all of them as ordinary draws.
    // !insideIf/!insideWhile: every skeleton here is a complete, self-
    // contained algorithm that writes its own final answer unconditionally,
    // exactly once -- it was never designed to run only when some unrelated
    // randomly-generated condition happens to be true (insideIf), nor to
    // run however many times (0, 1, or many) an unrelated enclosing loop's
    // own randomly-generated condition/counter happens to iterate
    // (insideWhile). Either context makes the skeleton's correctness
    // depend on an unrelated, uncontrolled random accident instead of
    // running deterministically once, so it could never reach a full score
    // and never survive regardless of the block-termination fix just
    // above. Same evidence trail as that fix: with block-termination and
    // the insideIf guard both in place, draw counts stayed high
    // (11012/109476 in one instrumented contains_ordered run) but
    // survivors stayed at zero.
    if (!insideIf && !insideWhile && canLoop && cfg.numVars >= 3 && cfg.loopAccumulatorBias > 0.0 &&
        rng.chance(cfg.loopAccumulatorBias)) {
        gSkeletonDrawAttempts++;
        enum class Skeleton { SelectionSort, HeapSort, ThreeSum, GraphReachability, ShortestPath, TspBruteForce, TspBranchAndBound, Dtw, BinarySearch, MergeSort, ScalarLoop };
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
        // Companion research-only override, same non-production status as
        // FORCE above: excludes exactly one named shape instead of
        // restricting to exactly one, for testing whether a NEW skeleton
        // was actually necessary or whether the rest of the existing pool
        // (plus crossover/mutation) can already reach a comparable result
        // without it (see data/skeleton_middle_ground_experiments.md,
        // Test 1). Mutually exclusive in practice with FORCE (setting both
        // to the same name yields zero options, same as any other
        // budget-incompatible name). Accepts a comma-separated list, so a
        // whole family (e.g. "selectionsort,heapsort,mergesort") can be
        // excluded at once.
        const char* disabledSkeletonEnv = std::getenv("ALGOSEARCH_DISABLE_SKELETON");
        auto isDisabled = [&](const char* name) {
            if (disabledSkeletonEnv == nullptr) return false;
            const size_t len = std::strlen(name);
            for (const char* p = disabledSkeletonEnv; *p != '\0';) {
                const char* end = std::strchr(p, ',');
                const size_t tokenLen = end != nullptr ? static_cast<size_t>(end - p) : std::strlen(p);
                if (tokenLen == len && std::strncmp(p, name, len) == 0) return true;
                if (end == nullptr) break;
                p = end + 1;
            }
            return false;
        };
        auto allowed = [&](const char* name) {
            if (isDisabled(name)) return false;
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
        WeightedOption options[18];
        int n = 0;
        int totalWeight = 0;
        auto addOption = [&](Skeleton s, int w) {
            options[n++] = {s, w};
            totalWeight += w;
        };
        if (canNestedLoop && cfg.numVars >= 4 && allowed("selectionsort")) addOption(Skeleton::SelectionSort, 3);
        if (canQuadLoop && cfg.numVars >= 5 && allowed("heapsort")) addOption(Skeleton::HeapSort, 1);
        if (canTripleLoop && cfg.numVars >= 5 && allowed("threesum")) addOption(Skeleton::ThreeSum, 3);
        if (canTripleLoop && cfg.numVars >= 6 && allowed("graphreachability")) addOption(Skeleton::GraphReachability, 3);
        if (canTripleLoop && cfg.numVars >= 7 && allowed("shortestpath")) addOption(Skeleton::ShortestPath, 3);
        // TSP brute force needs 4 While loops (init + identity-permutation
        // cost eval + Heap's-algorithm outer loop + its nested cost-eval
        // loop) and 11 scratch scalars -- weighted like HeapSort (1/3 of a
        // normal skeleton's share): it's the biggest AST here by far (an
        // exponential-enumeration shape, not a fixed-round relaxation), so
        // giving it an equal draw would slow down every OTHER problem's
        // search too, the same lesson HeapSort's weight comment already
        // documents above.
        if (canQuadLoop && cfg.numVars >= 12 && allowed("tspbruteforce")) addOption(Skeleton::TspBruteForce, 1);
        // Branch-and-bound needs only 2 loops (canNestedLoop, not
        // canQuadLoop) but the same 11-variable budget as brute force;
        // weighted the same low 1 as brute force for the same reason
        // (biggest ASTs here, would slow every other problem's search if
        // drawn at a normal skeleton's rate).
        if (canNestedLoop && cfg.numVars >= 12 && allowed("tspbranchandbound")) addOption(Skeleton::TspBranchAndBound, 1);
        if (canTripleLoop && cfg.numVars >= 12 && allowed("dtw")) addOption(Skeleton::Dtw, 3);
        if (canLoop && cfg.numVars >= 9 && allowed("binarysearch")) addOption(Skeleton::BinarySearch, 3);
        // Requires recursionCallBias > 0.0 itself (not just numVars/loop
        // budget): v1/v2 are only seeded with lo/hi at the interpreter
        // level when that's on (see Interpreter's seedRecursionBounds), so
        // offering this skeleton without it would just be dead weight that
        // always resets to whatever v1/v2 happen to hold (0/0 unless the
        // program itself sets them).
        // Fully hand-written control (see genMergeSortSkeletonBlock's
        // comment): same recursionCallBias gate as DivideConquer, plus the
        // 4 extra fixed locals (mid/i/j/k) its merge step needs.
        if (cfg.numVars >= 7 && cfg.recursionCallBias > 0.0 && allowed("mergesort")) addOption(Skeleton::MergeSort, 3);
        // Section 100: the generic scalar-loop piece (2-4 vars, one loop). It
        // replaced the recurrence, nesteddivisor, nesteddivisornative and
        // repeatedmultiply skeletons (deleted after section 100's joint battery).
        if (cfg.scalarLoop && cfg.numVars >= 3 && allowed("scalarloop")) addOption(Skeleton::ScalarLoop, 3);
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
            if (chosen == Skeleton::BinarySearch) gSkeletonBinarySearchPicks++;
            StmtPtr skeletonBlock = [&]() -> StmtPtr {
            switch (chosen) {
                case Skeleton::SelectionSort:
                    return genSelectionSortSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::HeapSort:
                    return genHeapSortSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::ThreeSum:
                    return genThreeSumSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::GraphReachability:
                    return genGraphReachabilitySkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::ShortestPath:
                    return genShortestPathSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::TspBruteForce:
                    return genTspBruteForceSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::TspBranchAndBound:
                    return genTspBranchAndBoundSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::Dtw:
                    return genDtwSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::BinarySearch:
                    return genBinarySearchSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::MergeSort:
                    return genMergeSortSkeletonBlock(rng, cfg, loopsUsed);
                case Skeleton::ScalarLoop:
                    return genScalarLoopBlock(rng, cfg, loopsUsed);
            }
            return nullptr;
            }();
            // Section 66: shape only -- every in-grammar site drawn at random.
            if (cfg.skeletonHoles && skeletonBlock) scrambleHoleSites(*skeletonBlock, rng);
            if (skeletonBlock) return skeletonBlock;
        }
    }

    // Composite primitives (Moção 2, Etapa 3): independent of the skeleton
    // draw above, so it can fire even at loopAccumulatorBias=0.0. Opt-in via
    // GenConfig::compositeBias. Needs 2 free loop slots (canNestedLoop) and 6
    // scratch vars for the shared binding context (the 6th, `found`, backs
    // the scoped-conjunction relation added for the substring isolation
    // follow-up -- FINDINGS.md section 31/32). pickDistinctVars draws from a
    // pool of size numVars-1 (it never returns v0), so numVars must be at
    // least one more than the var count requested (6) or the draw loop can
    // never terminate -- gated at 8, not 6, for headroom (learned the hard
    // way: numVars=6 here hung tests/generator_tests.cpp's
    // composite_bias_emits_bound_nested_loops for hours before this fix).
    if (canNestedLoop && cfg.numVars >= 8 && cfg.compositeBias > 0.0 && rng.chance(cfg.compositeBias)) {
        return genCompositeBlock(rng, cfg, loopsUsed);
    }

    // Combinator algebra (idea B, FINDINGS.md section 38): independent of
    // both draws above, so it can fire at loopAccumulatorBias=0.0. Like the
    // dedicated skeletons after section 30's fix, it is a self-contained
    // computation (it writes its own answer), so it only fires in a context
    // guaranteed to run exactly once. Same 6-var binding context and
    // numVars >= 8 headroom as genCompositeBlock.
    if (canLoop && !insideIf && !insideWhile && cfg.numVars >= 8 && cfg.combinatorBias > 0.0 &&
        rng.chance(cfg.combinatorBias)) {
        return genCombinatorBlock(rng, cfg, loopsUsed);
    }

    // Recursion extension (see GenConfig::recursionCallBias): checked before
    // the ordinary statement-kind choice below so a Call can appear
    // anywhere a generic statement can (top level or nested in if/while),
    // same pattern as indexIncrementBias/controlFlowBias overriding the
    // plain-assignment cases. Off by default (0.0), so this never fires
    // for any existing problem.
    if (cfg.recursionCallBias > 0.0 && cfg.numVars >= 3 && rng.chance(cfg.recursionCallBias)) {
        return makeCall(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
    }

    int maxChoice = canNest ? (canLoop ? 4 : 3) : 1;
    int choice = rng.uniformInt(0, maxChoice);

    switch (choice) {
        case 0:
            if (insideWhile && cfg.indexIncrementBias > 0.0 && rng.chance(cfg.indexIncrementBias)) {
                return genIndexIncrementStmt(rng, cfg);
            }
            return genPlainAssign(rng, cfg);
        case 1: {
            double swapChance = insideIf ? 0.5 : std::max(0.05, 0.5 - cfg.bareSwapPenalty);
            if (!rng.chance(swapChance)) {
                if (cfg.secondArrayBias > 0.0 && rng.chance(cfg.secondArrayBias)) {
                    return genPlainArrayAssign(rng, cfg, /*aux=*/true);
                }
                return genPlainArrayAssign(rng, cfg, /*aux=*/false);
            }
            return genPlainSwap(rng, cfg);
        }
        case 2: {
            // insideWhile is threaded through (not hard-reset to false) so
            // an if directly inside a loop still counts as "inside the
            // loop" for controlFlowBias -- `if (cond) continue;` is the
            // shape pruning actually needs, not a bare unconditional
            // continue as a loop's own top-level statement. Existing
            // indexIncrementBias (also insideWhile-gated) is unaffected by
            // default since both stay off (0.0) unless explicitly enabled.
            auto thenBody = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/true,
                                           insideWhile);
            std::vector<StmtPtr> elseBody;
            if (rng.chance(0.4))
                elseBody = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/true,
                                          insideWhile);
            return makeIf(genStmtCondExpr(rng, cfg), std::move(thenBody), std::move(elseBody));
        }
        case 3:
            if (insideWhile && cfg.indexIncrementBias > 0.0 && rng.chance(cfg.indexIncrementBias)) {
                return genIndexIncrementStmt(rng, cfg);
            }
            return genPlainAssign(rng, cfg);
        case 4: {
            loopsUsed++;
            auto body = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock, /*insideIf=*/false,
                                       /*insideWhile=*/true);
            return makeWhile(genStmtCondExpr(rng, cfg), std::move(body));
        }
        default:
            return genPlainAssign(rng, cfg);
    }
}

std::vector<StmtPtr> genBlockStmts(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, int maxStmts,
                                    bool insideIf, bool insideWhile) {
    std::vector<StmtPtr> stmts;
    int n = rng.uniformInt(1, std::max(1, maxStmts));
    for (int i = 0; i < n; ++i) {
        stmts.push_back(genStmt(rng, depthBudget, cfg, loopsUsed, insideIf, insideWhile));
        // Every dedicated skeleton (loopAccumulatorBias's gen*SkeletonBlock
        // draws, plus genCompositeBlock) is the ONLY statement kind genStmt
        // ever returns as StmtKind::Block -- every ordinary path returns
        // Assign/Swap/AssignArray/If/While/Call/Continue/Break instead (see
        // genStmt's case 0-4/default above). A skeleton is a complete,
        // self-contained algorithm that already writes its own final answer
        // (arr[0] = ..., or equivalent) -- it was never meant to be one
        // ingredient among several siblings in the same statement list.
        // Before this check, a random sibling generated after it (most
        // commonly another `arr[0] = ...` assignment, the single most
        // ordinary "write the output" idiom in this grammar) would silently
        // clobber the skeleton's correct result, and there was nothing
        // distinguishing "wrong because genuinely wrong" from "wrong
        // because a correct algorithm got overwritten by its own sibling"
        // in scoring. Confirmed as the actual reason
        // genBinarySearchSkeletonBlock never got picked up unforced despite
        // firing ~11% of the time it was eligible (16116/145681 draws in one
        // instrumented run, ALGOSEARCH_DEBUG_SKELETON_COUNTS) -- raising
        // numVars well past what the skeleton needs (24 vs. its 8) changed
        // nothing, which rules out variable-name collision as the cause and
        // points squarely at arr[0] being clobbered by a later sibling.
        // Stopping the block here is strictly corrective, not a new bias:
        // it only removes statements that were overwhelmingly likely to
        // just destroy a correct skeleton's output, never statements a
        // skeleton actually depended on (skeletons never read state a
        // still-to-be-generated sibling would have produced).
        if (stmts.back()->kind == StmtKind::Block) break;
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

// Collects mutable pointers to every if/while node in the tree, for
// GenConfig::exprMutationRate -- fine-grained mutation targets whose
// `cond` can be regenerated in place without touching `body`/`elseBody`.
void collectCondSites(Stmt& s, std::vector<Stmt*>& out) {
    if (s.kind == StmtKind::If || s.kind == StmtKind::While) out.push_back(&s);
    for (auto& c : s.body) collectCondSites(*c, out);
    for (auto& c : s.elseBody) collectCondSites(*c, out);
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

BinarySearchWithDecision buildBinarySearchWithDecision(const std::vector<int>& vars, ExprPtr decision, StmtPtr thenMove,
                                                       StmtPtr elseMove) {
    const int target = vars[0], lo = vars[1], v = vars[4], found = vars[6];
    std::vector<StmtPtr> foundBody;
    foundBody.push_back(makeAssign(found, makeIntConst(1)));
    foundBody.push_back(makeAssign(vars[2], makeVar(lo)));  // hi = lo: forces loop exit
    return buildBinarySearchStage2(vars, makeCompare(ExprKind::Eq, makeVar(v), makeVar(target)), std::move(foundBody),
                                   std::move(decision), std::move(thenMove), std::move(elseMove));
}

BinarySearchWithDecision buildBinarySearchStage2(const std::vector<int>& vars, ExprPtr foundCheck,
                                                 std::vector<StmtPtr> foundBody, ExprPtr decision, StmtPtr thenMove,
                                                 StmtPtr elseMove) {
    const int target = vars[0], lo = vars[1], hi = vars[2], mid = vars[3], v = vars[4], dir = vars[5], found = vars[6];
    auto root = makeBlock();
    root->body.push_back(makeAssign(target, makeArrayAccess(makeIntConst(0))));
    root->body.push_back(makeAssign(lo, makeIntConst(1)));
    root->body.push_back(makeAssign(hi, makeVar(0)));
    root->body.push_back(makeAssign(found, makeIntConst(0)));
    root->body.push_back(makeAssign(dir, makeIntConst(1)));
    {
        std::vector<StmtPtr> flip;
        flip.push_back(makeAssign(dir, makeBinOp(ExprKind::Sub, makeIntConst(0), makeIntConst(1))));
        root->body.push_back(makeIf(makeCompare(ExprKind::Gt, makeArrayAccess(makeIntConst(1)),
                                                makeArrayAccess(makeBinOp(ExprKind::Sub, makeVar(0), makeIntConst(1)))),
                                    std::move(flip)));
    }
    std::vector<StmtPtr> loopBody;
    loopBody.push_back(makeAssign(
        mid, makeBinOp(ExprKind::Add, makeVar(lo), makeBinOp(ExprKind::Div, makeBinOp(ExprKind::Sub, makeVar(hi), makeVar(lo)), makeIntConst(2)))));
    loopBody.push_back(makeAssign(v, makeArrayAccess(makeVar(mid))));
    std::vector<StmtPtr> thenBody, elseBody;
    thenBody.push_back(std::move(thenMove));
    elseBody.push_back(std::move(elseMove));
    auto decisionIf = makeIf(std::move(decision), std::move(thenBody), std::move(elseBody));
    const Stmt* site = decisionIf.get();
    std::vector<StmtPtr> notFound;
    notFound.push_back(std::move(decisionIf));
    auto foundIf = makeIf(std::move(foundCheck), std::move(foundBody), std::move(notFound));
    const Stmt* foundSite = foundIf.get();
    loopBody.push_back(std::move(foundIf));
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(lo), makeVar(hi)), std::move(loopBody)));
    root->body.push_back(makeAssignArray(makeIntConst(0), makeVar(found)));
    return {std::move(root), site, foundSite};
}

TspBranchAndBound buildTspBranchAndBoundWithGuard(const std::vector<int>& vars, ExprPtr guard) {
    int loopsUsed = 0;
    return buildTspBranchAndBoundImpl(vars, std::move(guard), loopsUsed);
}

Program generateRandomProgram(Random& rng, int targetSize, const GenConfig& cfg) {
    auto root = makeBlock();
    int loopsUsed = 0;
    int guard = 0;
    while (root->size() < targetSize && guard < 200) {
        guard++;
        root->body.push_back(genStmt(rng, cfg.maxBlockDepth, cfg, loopsUsed));
        if (static_cast<int>(root->body.size()) > cfg.maxStatementsPerBlock * 4) break;
        // Section 30's sibling-clobbering fix, applied at the program root
        // too, but only with combinatorBias on (keeps every other mode
        // byte-identical): a self-contained block writes its own answer, and
        // a later sibling like `arr[0] = ...` would silently overwrite it.
        if (cfg.combinatorBias > 0.0 && root->body.back()->kind == StmtKind::Block) break;
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

    // Fine-grained expression-level point mutation (see
    // GenConfig::exprMutationRate): regenerate ONE existing if/while's
    // condition in place, leaving its body/else-body untouched. Unlike
    // the whole-statement mutation above, this can tweak a comparison
    // (e.g. a pruning guard) without destroying whatever correct logic is
    // nested inside it.
    if (cfg.exprMutationRate > 0.0 && rng.chance(cfg.exprMutationRate)) {
        std::vector<Stmt*> condSites;
        collectCondSites(*root, condSites);
        if (!condSites.empty()) {
            Stmt* target = condSites[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(condSites.size()) - 1))];
            target->cond = genStmtCondExpr(rng, cfg);
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
