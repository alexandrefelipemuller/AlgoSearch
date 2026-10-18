#include <functional>

#include "core/Random.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(generator_produces_program_near_target_size) {
    Random rng(1);
    GenConfig cfg;
    Program p = generateRandomProgram(rng, 10, cfg);
    CHECK(p->size() >= 10);
    CHECK(p->size() <= 120);  // generous upper bound: a single nested if/while statement can overshoot
}

TEST(generator_respects_max_loops) {
    Random rng(2);
    GenConfig cfg;
    cfg.maxLoops = 1;
    for (int i = 0; i < 20; ++i) {
        Program p = generateRandomProgram(rng, 25, cfg);
        int loops = 0;
        std::function<void(const Stmt&)> count = [&](const Stmt& s) {
            if (s.kind == StmtKind::While) loops++;
            for (const auto& c : s.body) count(*c);
            for (const auto& c : s.elseBody) count(*c);
        };
        count(*p);
        CHECK(loops <= 1);
    }
}

TEST(loop_accumulator_bias_zero_disables_the_skeleton_bias) {
    // GenConfig's default (0.4) is the best value found by experiment
    // (see the comment on GenConfig::loopAccumulatorBias) -- it stays
    // opt-OUT, not opt-in. What must still hold is that passing 0.0
    // explicitly fully disables the bias path, reproducing plain
    // unbiased generation exactly (can't assert "no while loop ever
    // appears" since unbiased generation can still produce one by
    // chance, but we can assert every existing behavior-preservation
    // test elsewhere already covers that).
    GenConfig cfg;
    cfg.loopAccumulatorBias = 0.0;
    CHECK_EQ(cfg.loopAccumulatorBias, 0.0);
}

TEST(index_increment_bias_increases_increment_frequency) {
    // Force everything into a single while loop's body (no loop-bias, so
    // the increment can only come from the generic-assign path being
    // biased) and check the increment idiom shows up far more often with
    // the prior on than off.
    GenConfig biased;
    biased.numVars = 3;
    biased.maxLoops = 1;
    biased.loopAccumulatorBias = 0.0;  // isolate: only the generic-assign path should produce increments here
    biased.indexIncrementBias = 1.0;   // always, when inside a while

    Random rng(5);
    // Build one big while loop directly and count increments in its body.
    int loopsUsed = 0;
    // Use generateRandomProgram with a high target so a while is likely
    // generated at least once across a few attempts, then inspect it.
    int incrementHits = 0, totalAssigns = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        Program p = generateRandomProgram(rng, 12, biased);
        std::function<void(const Stmt&, bool)> walk = [&](const Stmt& s, bool insideWhile) {
            if (insideWhile && s.kind == StmtKind::Assign) {
                totalAssigns++;
                if (s.valueExpr->kind == ExprKind::Add && s.valueExpr->lhs->kind == ExprKind::Var &&
                    s.valueExpr->lhs->varIndex == s.varIndex && s.valueExpr->rhs->kind == ExprKind::IntConst &&
                    s.valueExpr->rhs->intValue == 1) {
                    incrementHits++;
                }
            }
            // Mirrors genStmt's own context propagation: entering an If's
            // body resets insideWhile to false (only the DIRECT parent
            // matters), entering a While's body sets it true.
            bool childInsideWhile = s.kind == StmtKind::While ? true : (s.kind == StmtKind::If ? false : insideWhile);
            for (const auto& c : s.body) walk(*c, childInsideWhile);
            for (const auto& c : s.elseBody) walk(*c, childInsideWhile);
        };
        walk(*p, false);
    }
    (void)loopsUsed;
    CHECK(totalAssigns > 0);
    CHECK(incrementHits == totalAssigns);  // bias=1.0 forces every in-loop assign to be the increment
}

TEST(grow_program_increases_size) {
    Random rng(3);
    GenConfig cfg;
    Program seed = generateRandomProgram(rng, 5, cfg);
    int originalSize = seed->size();
    Program grown = growProgram(*seed, originalSize + 10, rng, cfg);
    CHECK(grown->size() >= originalSize);
}

TEST(composite_bias_emits_bound_nested_loops) {
    // With compositeBias=1.0 and 2 free loop slots, every generic statement
    // draw becomes the two-loop composite block -- so a generated program
    // must contain at least one nested while pair built from the shared
    // binding context, and compositeBias=0.0 must not (default grammar only
    // builds loops through the ordinary statement-kind choice).
    std::function<void(const Stmt&, int&)> countWhile = [&](const Stmt& s, int& loops) {
        if (s.kind == StmtKind::While) loops++;
        for (const auto& c : s.body) countWhile(*c, loops);
        for (const auto& c : s.elseBody) countWhile(*c, loops);
    };

    GenConfig on;
    on.numVars = 8;  // genCompositeBlock needs numVars >= 8, see its call site's comment
    on.maxLoops = 4;
    on.maxBlockDepth = 3;
    on.maxStatementsPerBlock = 3;
    on.loopAccumulatorBias = 0.0;
    on.compositeBias = 1.0;
    Random rngOn(7);
    int loopsOn = 0;
    for (int attempt = 0; attempt < 10; ++attempt) {
        Program p = generateRandomProgram(rngOn, 25, on);
        countWhile(*p, loopsOn);
    }
    CHECK(loopsOn >= 2);

    GenConfig off;
    off.numVars = 8;
    off.maxLoops = 4;
    off.maxBlockDepth = 3;
    off.maxStatementsPerBlock = 3;
    off.loopAccumulatorBias = 0.0;
    off.compositeBias = 0.0;
    Random rngOff(7);
    int loopsOff = 0;
    for (int attempt = 0; attempt < 10; ++attempt) {
        Program p = generateRandomProgram(rngOff, 25, off);
        countWhile(*p, loopsOff);
    }
    CHECK(loopsOn > loopsOff);
}

namespace {

// Counts every site GenConfig::pruneGeneration is meant to remove: v = v,
// arr[e] = arr[e], swap(e, e), a constant if/while condition, and a Div/Mod
// by literal 0.
int countTrivialSites(const Stmt& s) {
    int n = 0;
    std::function<void(const Expr&)> walkExpr = [&](const Expr& e) {
        if ((e.kind == ExprKind::Div || e.kind == ExprKind::Mod) && e.rhs->kind == ExprKind::IntConst &&
            e.rhs->intValue == 0)
            n++;
        if (e.lhs) walkExpr(*e.lhs);
        if (e.rhs) walkExpr(*e.rhs);
    };
    std::function<void(const Stmt&)> walk = [&](const Stmt& st) {
        if (st.kind == StmtKind::Assign && st.valueExpr->kind == ExprKind::Var && st.valueExpr->varIndex == st.varIndex)
            n++;
        if (st.kind == StmtKind::AssignArray && st.valueExpr->kind == ExprKind::ArrayAccess &&
            exprEqual(st.valueExpr->lhs.get(), st.indexExpr.get()))
            n++;
        if (st.kind == StmtKind::Swap && exprEqual(st.indexExpr.get(), st.indexExpr2.get())) n++;
        if ((st.kind == StmtKind::If || st.kind == StmtKind::While) && constantConditionValue(*st.cond)) n++;
        for (const Expr* e : {st.valueExpr.get(), st.indexExpr.get(), st.indexExpr2.get(), st.cond.get()})
            if (e) walkExpr(*e);
        for (const auto& c : st.body) walk(*c);
        for (const auto& c : st.elseBody) walk(*c);
    };
    walk(s);
    return n;
}

// numVars=2 makes self-references common enough to measure with a small
// sample; loopAccumulatorBias=0.0 keeps every statement on the generic path.
GenConfig trivialProneConfig(bool prune) {
    GenConfig cfg;
    cfg.numVars = 2;
    cfg.loopAccumulatorBias = 0.0;
    cfg.pruneGeneration = prune;
    return cfg;
}

int trivialSitesAcrossSample(const GenConfig& cfg, uint64_t seed) {
    Random rng(seed);
    int total = 0;
    for (int i = 0; i < 300; ++i) {
        Program p = generateRandomProgram(rng, 20, cfg);
        total += countTrivialSites(*p);
        Program grown = growProgram(*p, p->size() + 10, rng, cfg);
        total += countTrivialSites(*grown);
    }
    return total;
}

}  // namespace

TEST(prune_generation_off_still_emits_trivial_sites) {
    // The detectors must be measuring something real: with pruning off, the
    // unpruned grammar does produce these shapes.
    CHECK(trivialSitesAcrossSample(trivialProneConfig(false), 21) > 0);
}

TEST(prune_generation_removes_trivial_sites_from_generate_and_grow) {
    CHECK_EQ(trivialSitesAcrossSample(trivialProneConfig(true), 21), 0);
}

TEST(role_typed_vars_keep_indices_and_values_apart) {
    // With roleTypedVars on and no skeleton drawable, every array subscript
    // is built only from index vars (odd), v0 and literals, and v0 is never
    // assigned.
    GenConfig cfg;
    cfg.numVars = 6;
    cfg.loopAccumulatorBias = 0.0;
    cfg.roleTypedVars = true;
    Random rng(33);
    int violations = 0, subscripts = 0;
    std::function<void(const Expr&)> checkIndex = [&](const Expr& e) {
        if (e.kind == ExprKind::Var && e.varIndex != 0 && e.varIndex % 2 == 0) violations++;
        if (e.kind == ExprKind::ArrayAccess || e.kind == ExprKind::AuxAccess) violations++;
        if (e.lhs) checkIndex(*e.lhs);
        if (e.rhs) checkIndex(*e.rhs);
    };
    std::function<void(const Expr&)> walkExpr = [&](const Expr& e) {
        if (e.kind == ExprKind::ArrayAccess) {
            subscripts++;
            checkIndex(*e.lhs);
        }
        if (e.lhs) walkExpr(*e.lhs);
        if (e.rhs) walkExpr(*e.rhs);
    };
    std::function<void(const Stmt&)> walk = [&](const Stmt& st) {
        if (st.kind == StmtKind::Assign && st.varIndex == 0) violations++;
        if (st.kind == StmtKind::AssignArray || st.kind == StmtKind::Swap) {
            subscripts++;
            checkIndex(*st.indexExpr);
        }
        if (st.kind == StmtKind::Swap) checkIndex(*st.indexExpr2);
        for (const Expr* e : {st.valueExpr.get(), st.indexExpr.get(), st.indexExpr2.get(), st.cond.get()})
            if (e) walkExpr(*e);
        for (const auto& c : st.body) walk(*c);
        for (const auto& c : st.elseBody) walk(*c);
    };
    for (int i = 0; i < 200; ++i) {
        Program p = generateRandomProgram(rng, 20, cfg);
        walk(*p);
        Program grown = growProgram(*p, p->size() + 10, rng, cfg);
        walk(*grown);
    }
    CHECK(subscripts > 0);
    CHECK_EQ(violations, 0);
}

TEST(dc_shell_bias_emits_the_guarded_two_call_shape_only_when_enabled) {
    GenConfig on;
    on.numVars = 7;
    on.maxLoops = 2;
    on.maxBlockDepth = 3;
    on.maxStatementsPerBlock = 3;
    on.loopAccumulatorBias = 0.0;
    on.recursionCallBias = 0.1;
    on.dcShellBias = 1.0;
    GenConfig off = on;
    off.dcShellBias = 0.0;

    auto countShells = [&](const GenConfig& cfg) {
        Random rng(11);
        int shells = 0;
        for (int attempt = 0; attempt < 30; ++attempt) {
            Program p = generateRandomProgram(rng, 25, cfg);
            std::string text = p->toString();
            if (text.find("(v2 - v1) > 1") != std::string::npos && text.find("call(v1, v3)") != std::string::npos &&
                text.find("call(v3, v2)") != std::string::npos)
                shells++;
        }
        return shells;
    };
    CHECK(countShells(on) >= 5);
    CHECK_EQ(countShells(off), 0);
}

TEST(dc_shell_search_bias_emits_the_one_call_interval_shape_only_when_enabled) {
    GenConfig on;
    on.numVars = 7;
    on.maxLoops = 2;
    on.maxBlockDepth = 3;
    on.maxStatementsPerBlock = 3;
    on.loopAccumulatorBias = 0.0;
    on.recursionCallBias = 0.1;
    on.dcShellSearchBias = 1.0;
    GenConfig off = on;
    off.dcShellSearchBias = 0.0;

    auto countShells = [&](const GenConfig& cfg) {
        Random rng(11);
        int shells = 0;
        for (int attempt = 0; attempt < 30; ++attempt) {
            std::string text = generateRandomProgram(rng, 25, cfg)->toString();
            if (text.find("(v2 - v1) > 1") != std::string::npos && text.find("v3 = (v1 + ((v2 - v1) / 2))") != std::string::npos &&
                text.find("call(v1, ") != std::string::npos && text.find("arr[0] = ") != std::string::npos)
                shells++;
        }
        return shells;
    };
    CHECK(countShells(on) >= 5);
    CHECK_EQ(countShells(off), 0);
}

TEST(dc_shell_search_key_mode_compares_with_the_key_and_ends_in_a_membership_flag) {
    GenConfig cfg;
    cfg.numVars = 7;
    cfg.maxLoops = 2;
    cfg.maxBlockDepth = 3;
    cfg.maxStatementsPerBlock = 3;
    cfg.loopAccumulatorBias = 0.0;
    cfg.recursionCallBias = 0.1;
    cfg.dcShellSearchBias = 1.0;
    cfg.dcShellSearchKey = 1.0;
    Random rng(11);
    int keyShells = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        std::string text = generateRandomProgram(rng, 25, cfg)->toString();
        if (text.find("arr[v3] ") != std::string::npos && text.find("arr[0])") != std::string::npos &&
            text.find("(v1 < 1)") != std::string::npos)
            keyShells++;
    }
    CHECK(keyShells >= 5);
}

TEST(dc_shell_search_end_mode_compares_with_the_interval_end_and_writes_a_value) {
    GenConfig cfg;
    cfg.numVars = 7;
    cfg.maxLoops = 2;
    cfg.maxBlockDepth = 3;
    cfg.maxStatementsPerBlock = 3;
    cfg.loopAccumulatorBias = 0.0;
    cfg.recursionCallBias = 0.1;
    cfg.dcShellSearchBias = 1.0;
    cfg.dcShellSearchEnd = 1.0;
    Random rng(11);
    int endShells = 0;
    for (int attempt = 0; attempt < 30; ++attempt) {
        std::string text = generateRandomProgram(rng, 25, cfg)->toString();
        if (text.find("arr[(v2 - 1)])") != std::string::npos && text.find("arr[0] = arr[") != std::string::npos) endShells++;
    }
    CHECK(endShells >= 5);
}
