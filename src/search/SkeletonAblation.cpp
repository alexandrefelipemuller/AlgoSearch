#include "search/SkeletonAblation.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "search/ProgramGenerator.hpp"

namespace algosearch {

namespace {

// One hole position: the k-th condition or assignment value in traversal order.
struct Site {
    bool isCond;
    std::string original;
};

// Visits every hole slot of `s` in a fixed order; `fn(stmt, slot, isCond)`.
void forEachSlot(Stmt& s, const std::function<void(Stmt&, ExprPtr&, bool)>& fn) {
    switch (s.kind) {
        case StmtKind::Assign:
        case StmtKind::AssignArray:
            fn(s, s.valueExpr, false);
            break;
        case StmtKind::If:
        case StmtKind::While:
            fn(s, s.cond, true);
            break;
        default:
            break;
    }
    for (auto& c : s.body) forEachSlot(*c, fn);
    for (auto& c : s.elseBody) forEachSlot(*c, fn);
}

// Clone of `program` with site k replaced by `fill`; `siteStmt` receives the
// statement holding the hole (for tracing).
Program withHole(const Stmt& program, size_t k, const Expr* fill, const Stmt** siteStmt = nullptr) {
    Program copy = program.clone();
    size_t i = 0;
    forEachSlot(*copy, [&](Stmt& st, ExprPtr& slot, bool) {
        if (i++ != k) return;
        if (fill) slot = fill->clone();
        if (siteStmt) *siteStmt = &st;
    });
    return copy;
}

void collectVars(const Expr& e, std::set<int>& out) {
    if (e.kind == ExprKind::Var) out.insert(e.varIndex);
    if (e.lhs) collectVars(*e.lhs, out);
    if (e.rhs) collectVars(*e.rhs, out);
}

void collectVars(const Stmt& s, std::set<int>& out) {
    if (s.kind == StmtKind::Assign) out.insert(s.varIndex);
    for (const Expr* e : {s.valueExpr.get(), s.indexExpr.get(), s.indexExpr2.get(), s.cond.get()}) {
        if (e) collectVars(*e, out);
    }
    for (const auto& c : s.body) collectVars(*c, out);
    for (const auto& c : s.elseBody) collectVars(*c, out);
}

std::vector<ExprPtr> atomsFor(const std::vector<int>& vars, bool withArray) {
    std::vector<ExprPtr> atoms;
    for (int v : vars) atoms.push_back(makeVar(v));
    for (int c = 0; c <= 2; ++c) atoms.push_back(makeIntConst(c));
    if (withArray) {
        for (int v : vars) atoms.push_back(makeArrayAccess(makeVar(v)));
        for (int c = 0; c <= 2; ++c) atoms.push_back(makeArrayAccess(makeIntConst(c)));
    }
    return atoms;
}

std::vector<ExprPtr> enumerateValues(const std::vector<int>& vars) {
    std::vector<ExprPtr> atoms = atomsFor(vars, true), out;
    for (const auto& a : atoms) out.push_back(a->clone());
    const ExprKind ops[] = {ExprKind::Add, ExprKind::Sub, ExprKind::Mul, ExprKind::Div, ExprKind::Mod};
    for (const auto& a : atoms) {
        for (const auto& b : atoms) {
            if (a->kind == ExprKind::IntConst && b->kind == ExprKind::IntConst) continue;
            for (ExprKind op : ops) out.push_back(makeBinOp(op, a->clone(), b->clone()));
        }
    }
    return out;
}

std::vector<ExprPtr> enumerateConds(const std::vector<int>& vars) {
    std::vector<ExprPtr> atoms = atomsFor(vars, true), terms, out;
    for (const auto& a : atoms) terms.push_back(a->clone());
    for (const auto& a : atoms) {
        for (const auto& b : atoms) {
            if (a->kind == ExprKind::IntConst && b->kind == ExprKind::IntConst) continue;
            terms.push_back(makeBinOp(ExprKind::Add, a->clone(), b->clone()));
            terms.push_back(makeBinOp(ExprKind::Sub, a->clone(), b->clone()));
        }
    }
    const ExprKind ops[] = {ExprKind::Lt, ExprKind::Gt, ExprKind::Eq, ExprKind::Le, ExprKind::Ge, ExprKind::Ne};
    for (const auto& l : terms) {
        for (const auto& r : atoms) {
            for (ExprKind op : ops) out.push_back(makeCompare(op, l->clone(), r->clone()));
        }
    }
    return out;
}

// Value of a hole expression on a traced state; `err` on an out-of-range
// read or a division by zero.
long evalState(const Expr& e, const std::vector<int>& vars, const std::vector<int>& arr, bool& err) {
    switch (e.kind) {
        case ExprKind::IntConst: return e.intValue;
        case ExprKind::Var: return vars[static_cast<size_t>(e.varIndex)];
        case ExprKind::ArrayAccess: {
            long i = evalState(*e.lhs, vars, arr, err);
            if (err || i < 0 || i >= static_cast<long>(arr.size())) {
                err = true;
                return 0;
            }
            return arr[static_cast<size_t>(i)];
        }
        default: break;
    }
    long a = evalState(*e.lhs, vars, arr, err), b = evalState(*e.rhs, vars, arr, err);
    if (err) return 0;
    switch (e.kind) {
        case ExprKind::Add: return static_cast<int>(a + b);
        case ExprKind::Sub: return static_cast<int>(a - b);
        case ExprKind::Mul: return static_cast<int>(a * b);
        case ExprKind::Div:
        case ExprKind::Mod:
            if (b == 0) {
                err = true;
                return 0;
            }
            return e.kind == ExprKind::Div ? a / b : a % b;
        case ExprKind::Lt: return a < b;
        case ExprKind::Gt: return a > b;
        case ExprKind::Eq: return a == b;
        case ExprKind::Le: return a <= b;
        case ExprKind::Ge: return a >= b;
        case ExprKind::Ne: return a != b;
        default: err = true; return 0;
    }
}

struct State {
    std::vector<int> vars, arr;
};

std::vector<ExprPtr> dedupeOnStates(std::vector<ExprPtr> exprs, const std::vector<State>& states) {
    std::unordered_set<std::string> seen;
    std::vector<ExprPtr> reps;
    for (auto& e : exprs) {
        std::string key;
        for (const auto& st : states) {
            bool err = false;
            long v = evalState(*e, st.vars, st.arr, err);
            key += err ? "E," : std::to_string(v) + ",";
        }
        if (seen.insert(key).second) reps.push_back(std::move(e));
    }
    return reps;
}

}  // namespace

void runSkeletonAblation(const std::string& name, const Problem& problem, int startLevel, int trainMax, int numVars,
                         uint64_t seed, unsigned threads) {
    std::vector<Instance> training, heldOut;
    int level = startLevel;
    for (; level <= trainMax; level = problem.nextLevel(level)) {
        for (auto& inst : problem.generateInstances(level)) training.push_back(std::move(inst));
    }
    for (int k = 0; k < 3; ++k, level = problem.nextLevel(level)) {
        for (auto& inst : problem.generateInstances(level)) heldOut.push_back(std::move(inst));
        for (auto& inst : problem.generateAdversarialInstances(level)) heldOut.push_back(std::move(inst));
    }
    const long fullLimit = 5'000'000;
    auto passes = [&](const Stmt& prog, const std::vector<Instance>& insts, long limit, long* steps) {
        Interpreter interp(limit);
        std::vector<int> out;
        long total = 0;
        for (const auto& inst : insts) {
            long st = 0;
            if (problem.evaluateInto(prog, inst, interp, out, &st) != ExecStatus::SUCCESS) return false;
            total += st;
        }
        if (steps) *steps = total;
        return true;
    };

    // 1. Reference program.
    setenv("ALGOSEARCH_FORCE_SKELETON", name.c_str(), 1);
    GenConfig cfg;
    cfg.numVars = numVars;
    cfg.loopAccumulatorBias = 1.0;
    cfg.combinatorBias = 0.0;
    Random rng(seed);
    Program reference;
    for (int attempt = 0; attempt < 20000 && !reference; ++attempt) {
        Program p = generateRandomProgram(rng, 2, cfg);
        if (passes(*p, training, fullLimit, nullptr) && passes(*p, heldOut, fullLimit, nullptr)) reference = std::move(p);
    }
    unsetenv("ALGOSEARCH_FORCE_SKELETON");
    if (!reference) {
        std::cout << "[ablate] " << name << ": no reference program found in 20000 draws\n";
        return;
    }
    long refSteps = 0;
    passes(*reference, training, fullLimit, &refSteps);
    long maxPerInstance = 0;
    {
        Interpreter interp(fullLimit);
        std::vector<int> out;
        for (const auto& inst : training) {
            long st = 0;
            problem.evaluateInto(*reference, inst, interp, out, &st);
            maxPerInstance = std::max(maxPerInstance, st);
        }
    }
    const long filterLimit = std::max(20'000L, 4 * maxPerInstance);
    std::set<int> varSet = {0};
    collectVars(*reference, varSet);
    std::vector<int> vars(varSet.begin(), varSet.end());
    std::cout << "[ablate] " << name << ": reference (" << reference->size() << " nodes, " << refSteps << " training steps, "
              << training.size() << " training / " << heldOut.size() << " held-out instances)\n"
              << reference->toString() << "\n";

    // 2. Sites.
    std::vector<Site> sites;
    forEachSlot(*reference, [&](Stmt&, ExprPtr& slot, bool isCond) { sites.push_back({isCond, slot->toString()}); });

    int recovered = 0, reachOnly = 0, unreached = 0, inGrammar = 0;
    int condTotal = 0, condRecovered = 0, valueTotal = 0, valueRecovered = 0;
    for (size_t k = 0; k < sites.size(); ++k) {
        const Site& site = sites[k];
        // Trace the reference's states at this site.
        std::vector<State> states;
        {
            const Stmt* siteStmt = nullptr;
            Program traced = withHole(*reference, k, nullptr, &siteStmt);
            Interpreter interp(fullLimit);
            long visits = 0;
            interp.setConditionTrace(siteStmt, [&](const std::vector<int>& v, const std::vector<int>& a) {
                if (visits++ % 3 == 0 && states.size() < 3000) states.push_back({v, a});
            });
            std::vector<int> out;
            for (const auto& inst : training) interp.run(*traced, inst.input, numVars, out);
        }
        std::vector<ExprPtr> all = site.isCond ? enumerateConds(vars) : enumerateValues(vars);
        bool originalInGrammar = false;
        for (const auto& e : all) originalInGrammar = originalInGrammar || e->toString() == site.original;
        inGrammar += originalInGrammar;
        size_t enumerated = all.size();
        std::vector<ExprPtr> reps = dedupeOnStates(std::move(all), states);

        // Fast rejection with a per-thread killer order, then full steps.
        struct Hit {
            size_t rep;
            long steps;
        };
        std::vector<std::vector<Hit>> hits(threads);
        std::vector<std::thread> pool;
        for (unsigned t = 0; t < threads; ++t) {
            pool.emplace_back([&, t] {
                Interpreter interp(filterLimit);
                std::vector<int> out;
                std::vector<size_t> order(training.size());
                for (size_t i = 0; i < order.size(); ++i) order[i] = i;
                for (size_t r = t; r < reps.size(); r += threads) {
                    Program prog = withHole(*reference, k, reps[r].get());
                    bool ok = true;
                    for (size_t i = 0; i < order.size(); ++i) {
                        if (problem.evaluateInto(*prog, training[order[i]], interp, out) != ExecStatus::SUCCESS) {
                            std::rotate(order.begin(), order.begin() + static_cast<long>(i), order.begin() + static_cast<long>(i) + 1);
                            ok = false;
                            break;
                        }
                    }
                    long steps = 0;
                    if (ok && passes(*prog, training, fullLimit, &steps)) hits[t].push_back({r, steps});
                }
            });
        }
        for (auto& th : pool) th.join();
        std::vector<Hit> correct;
        for (auto& h : hits) correct.insert(correct.end(), h.begin(), h.end());
        std::sort(correct.begin(), correct.end(), [](const Hit& a, const Hit& b) {
            return a.steps != b.steps ? a.steps < b.steps : a.rep < b.rep;
        });

        bool tiedAllOk = !correct.empty(), anyOk = false;
        size_t tied = 0;
        std::string winner = correct.empty() ? "-" : reps[correct.front().rep]->toString();
        for (size_t i = 0; i < correct.size() && i < 300; ++i) {
            bool ok = passes(*withHole(*reference, k, reps[correct[i].rep].get()), heldOut, fullLimit, nullptr);
            anyOk = anyOk || ok;
            if (correct[i].steps == correct.front().steps) {
                tied++;
                tiedAllOk = tiedAllOk && ok;
            }
        }
        const char* verdict = tiedAllOk ? "RECOVERED" : anyOk ? "reach-only" : "unreached";
        if (tiedAllOk) recovered++;
        else if (anyOk) reachOnly++;
        else unreached++;
        (site.isCond ? condTotal : valueTotal)++;
        if (tiedAllOk) (site.isCond ? condRecovered : valueRecovered)++;
        std::cout << "[ablate] site " << k << " " << (site.isCond ? "cond " : "value") << " original " << site.original
                  << (originalInGrammar ? "" : " (NOT in grammar)") << ": " << enumerated << " -> " << reps.size()
                  << " classes, " << correct.size() << " training-correct, " << tied << " tied winner(s), e.g. " << winner
                  << " -> " << verdict << "\n";
    }
    std::cout << "[ablate] SUMMARY " << name << " on " << problem.describeLevel(trainMax) << ": " << sites.size() << " sites ("
              << inGrammar << " in grammar): recovered " << recovered << ", reach-only " << reachOnly << ", unreached "
              << unreached << " | cond " << condRecovered << "/" << condTotal << " recovered, value " << valueRecovered << "/"
              << valueTotal << " recovered\n";
}

}  // namespace algosearch
