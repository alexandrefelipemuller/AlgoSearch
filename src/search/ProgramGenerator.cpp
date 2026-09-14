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

ExprPtr genCondExpr(Random& rng, int depthBudget, const GenConfig& cfg) {
    ExprKind kinds[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq};
    ExprKind k = kinds[rng.uniformInt(0, 2)];
    return makeCompare(k, genValueExpr(rng, depthBudget, cfg), genValueExpr(rng, depthBudget, cfg));
}

std::vector<StmtPtr> genBlockStmts(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, int maxStmts);

// One canonical accumulation body for the counting-loop skeleton below.
// `vi` is the loop index variable, `acc` a second variable seeded before
// the loop. All five shapes touch arr[vi] -- the one thing every array
// algorithm (min/max/sum/contains/sort) actually needs a loop for.
StmtPtr genAccumulatorBodyStmt(Random& rng, int vi, int acc, const GenConfig& cfg) {
    int choice = rng.uniformInt(0, 4);
    switch (choice) {
        case 0:  // running sum: acc = acc + arr[vi]
            return makeAssign(acc, makeBinOp(ExprKind::Add, makeVar(acc), makeArrayAccess(makeVar(vi))));
        case 1:  // running min: if (arr[vi] < acc) acc = arr[vi]
        {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeArrayAccess(makeVar(vi))));
            return makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(makeVar(vi)), makeVar(acc)), std::move(then));
        }
        case 2:  // running max: if (arr[vi] > acc) acc = arr[vi]
        {
            std::vector<StmtPtr> then;
            then.push_back(makeAssign(acc, makeArrayAccess(makeVar(vi))));
            return makeIf(makeCompare(ExprKind::Gt, makeArrayAccess(makeVar(vi)), makeVar(acc)), std::move(then));
        }
        case 3:  // membership: if (arr[vi] == arr[0]) arr[0] = 1
        {
            std::vector<StmtPtr> then;
            then.push_back(makeAssignArray(makeIntConst(0), makeIntConst(1)));
            return makeIf(makeCompare(ExprKind::Eq, makeArrayAccess(makeVar(vi)), makeArrayAccess(makeIntConst(0))), std::move(then));
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

    auto root = makeBlock();
    root->body.push_back(makeAssign(acc, rng.chance(0.5) ? makeIntConst(0) : makeArrayAccess(makeIntConst(0))));
    root->body.push_back(makeAssign(vi, makeIntConst(0)));

    std::vector<StmtPtr> body;
    body.push_back(genAccumulatorBodyStmt(rng, vi, acc, cfg));
    if (rng.chance(0.3)) body.push_back(genAccumulatorBodyStmt(rng, vi, acc, cfg));
    body.push_back(makeAssign(vi, makeBinOp(ExprKind::Add, makeVar(vi), makeIntConst(1))));

    loopsUsed++;
    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(vi), makeVar(0)), std::move(body)));
    return root;
}

StmtPtr genStmt(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed) {
    // Weighted choice among available statement kinds.
    bool canNest = depthBudget > 0;
    bool canLoop = canNest && loopsUsed < cfg.maxLoops;

    if (canLoop && cfg.numVars >= 3 && cfg.loopAccumulatorBias > 0.0 && rng.chance(cfg.loopAccumulatorBias)) {
        return genCountingLoopBlock(rng, cfg, loopsUsed);
    }

    int maxChoice = canNest ? (canLoop ? 4 : 3) : 1;
    int choice = rng.uniformInt(0, maxChoice);

    switch (choice) {
        case 0:
            return makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
        case 1:
            if (rng.chance(0.5)) {
                return makeAssignArray(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
            }
            return makeSwap(genValueExpr(rng, cfg.maxExprDepth, cfg), genValueExpr(rng, cfg.maxExprDepth, cfg));
        case 2: {
            auto thenBody = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock);
            std::vector<StmtPtr> elseBody;
            if (rng.chance(0.4)) elseBody = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock);
            return makeIf(genCondExpr(rng, cfg.maxExprDepth, cfg), std::move(thenBody), std::move(elseBody));
        }
        case 3:
            return makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
        case 4: {
            loopsUsed++;
            auto body = genBlockStmts(rng, depthBudget - 1, cfg, loopsUsed, cfg.maxStatementsPerBlock);
            return makeWhile(genCondExpr(rng, cfg.maxExprDepth, cfg), std::move(body));
        }
        default:
            return makeAssign(rng.uniformInt(0, cfg.numVars - 1), genValueExpr(rng, cfg.maxExprDepth, cfg));
    }
}

std::vector<StmtPtr> genBlockStmts(Random& rng, int depthBudget, const GenConfig& cfg, int& loopsUsed, int maxStmts) {
    std::vector<StmtPtr> stmts;
    int n = rng.uniformInt(1, std::max(1, maxStmts));
    for (int i = 0; i < n; ++i) {
        stmts.push_back(genStmt(rng, depthBudget, cfg, loopsUsed));
    }
    return stmts;
}

int countLoops(const Stmt& s) {
    int total = (s.kind == StmtKind::While) ? 1 : 0;
    for (const auto& c : s.body) total += countLoops(*c);
    for (const auto& c : s.elseBody) total += countLoops(*c);
    return total;
}

// Collects mutable references to every block-like statement vector in the
// tree (the root's body, plus every if/while body and if's else-body).
void collectBlocks(Stmt& s, std::vector<std::vector<StmtPtr>*>& out) {
    // Only Block/If/While actually execute and print their `body`; only If
    // uses `elseBody`. Registering any other vector as an insertion target
    // would make inserted statements invisible to both execution and print.
    if (s.kind == StmtKind::Block || s.kind == StmtKind::If || s.kind == StmtKind::While) {
        out.push_back(&s.body);
    }
    if (s.kind == StmtKind::If) {
        out.push_back(&s.elseBody);
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
        std::vector<std::vector<StmtPtr>*> blocks;
        collectBlocks(*root, blocks);
        if (blocks.empty()) break;
        auto* target = blocks[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(blocks.size()) - 1))];
        size_t pos = target->empty() ? 0 : static_cast<size_t>(rng.uniformInt(0, static_cast<int>(target->size())));
        target->insert(target->begin() + static_cast<long>(pos), genStmt(rng, cfg.maxBlockDepth, cfg, loopsUsed));
    }

    // Point-mutate: replace one existing statement in place. This is what
    // lets beam search refine logic (not just append more of it) -- pure
    // insertion can only ever add code, never correct a wrong comparison or
    // wrong index deep inside an already-large candidate.
    if (rng.chance(0.6)) {
        std::vector<std::vector<StmtPtr>*> blocks;
        collectBlocks(*root, blocks);
        std::vector<std::vector<StmtPtr>*> nonEmpty;
        for (auto* b : blocks) {
            if (!b->empty()) nonEmpty.push_back(b);
        }
        if (!nonEmpty.empty()) {
            auto* target = nonEmpty[static_cast<size_t>(rng.uniformInt(0, static_cast<int>(nonEmpty.size()) - 1))];
            size_t idx = static_cast<size_t>(rng.uniformInt(0, static_cast<int>(target->size()) - 1));
            int loopsUsed = countLoops(*root);
            (*target)[idx] = genStmt(rng, cfg.maxBlockDepth, cfg, loopsUsed);
        }
    }
    return root;
}

}  // namespace algosearch
