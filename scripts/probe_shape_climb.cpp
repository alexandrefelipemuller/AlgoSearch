// FINDINGS.md sections 66-67: hill-climb scrambled skeletons (every site in
// the hole grammar drawn at random, --skeleton-holes) by exhaustive
// per-site enumeration, and -- with `pairs` -- by joint enumeration of site
// pairs whenever no single site improves. Reports, per trial, the score on
// the training levels before and after.
//
// Build against the library objects (after `make`):
//   g++ -std=c++20 -O2 -Isrc scripts/probe_shape_climb.cpp \
//       $(find build/lib -name '*.o' ! -name main.o) -o probe_shape_climb -lpthread
// Usage: ./probe_shape_climb gcd|factorial [pairs]

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

#include "interpreter/Interpreter.hpp"
#include "problems/FactorialProblem.hpp"
#include "problems/GcdProblem.hpp"
#include "search/HoleGrammar.hpp"
#include "search/ProgramGenerator.hpp"

using namespace algosearch;

namespace {

struct Score {
    int solved = 0;
    long steps = 0;
    bool beats(const Score& o) const { return solved > o.solved || (solved == o.solved && steps < o.steps); }
};

// Score on `insts`; with `bar`, gives up (returns false) once the remaining
// instances cannot lift the solved count to bar.solved.
bool scoreOn(const Problem& p, const Stmt& prog, const std::vector<Instance>& insts, Score& s, const Score* bar = nullptr) {
    Interpreter in(20000);
    std::vector<int> out;
    s = {};
    for (size_t i = 0; i < insts.size(); ++i) {
        long x = 0;
        if (p.evaluateInto(prog, insts[i], in, out, &x) == ExecStatus::SUCCESS) s.solved++;
        s.steps += x;
        if (bar && s.solved + static_cast<int>(insts.size() - i - 1) < bar->solved) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    std::string which = argc > 1 ? argv[1] : "gcd";
    bool pairs = argc > 2 && std::string(argv[2]) == "pairs";
    std::unique_ptr<Problem> p;
    std::string skeleton;
    if (which == "gcd") {
        p = std::make_unique<GcdProblem>(20, 40, 8);
        skeleton = "recurrence";
    } else {
        p = std::make_unique<FactorialProblem>(15, 40, 8);
        skeleton = "repeatedmultiply";
    }
    std::vector<Instance> train, sample;
    for (int l = 1; l <= 6; l = p->nextLevel(l)) {
        for (auto& i : p->generateInstances(l)) train.push_back(i);
    }
    size_t stride = std::max<size_t>(1, train.size() / 40);
    for (size_t i = 0; i < train.size(); i += stride) sample.push_back(train[i]);
    const std::vector<Instance>& climbSet = pairs ? sample : train;

    setenv("ALGOSEARCH_FORCE_SKELETON", skeleton.c_str(), 1);
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.loopAccumulatorBias = 1.0;
    cfg.combinatorBias = 0.0;
    cfg.skeletonHoles = true;
    unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    int full = 0;
    for (int trial = 0; trial < 20; ++trial) {
        Random rng(100 + static_cast<uint64_t>(trial));
        Program w = generateRandomProgram(rng, 2, cfg);
        Score cur, start;
        scoreOn(*p, *w, train, start);
        scoreOn(*p, *w, climbSet, cur);
        size_t sites = 0;
        forEachSlot(*w, [&](Stmt&, ExprPtr&, bool) { sites++; });
        std::set<int> vs = {0};
        collectVars(*w, vs);
        std::vector<int> vars(vs.begin(), vs.end());
        // Classes of every site, deduplicated on the current program's states.
        auto siteClasses = [&](size_t k) {
            bool isCond = false;
            size_t idx = 0;
            forEachSlot(*w, [&](Stmt&, ExprPtr&, bool c) {
                if (idx++ == k) isCond = c;
            });
            std::vector<HoleState> states;
            const Stmt* site = nullptr;
            Program traced = withHole(*w, k, nullptr, &site);
            Interpreter tracer(20000);
            tracer.setConditionTrace(site, [&](const std::vector<int>& v, const std::vector<int>& a) {
                if (states.size() < 300) states.push_back({v, a});
            });
            std::vector<int> out;
            for (const auto& i : climbSet) tracer.run(*traced, i.input, 8, out);
            return dedupeOnStates(isCond ? enumerateConds(vars) : enumerateValues(vars), states);
        };
        int pairMoves = 0;
        for (int round = 0; round < 40; ++round) {
            bool improved = false;
            for (size_t k = 0; k < sites; ++k) {
                for (const auto& f : siteClasses(k)) {
                    Program c = withHole(*w, k, f.get());
                    Score s;
                    if (scoreOn(*p, *c, climbSet, s, &cur) && s.beats(cur)) {
                        cur = s;
                        w = std::move(c);
                        improved = true;
                    }
                }
            }
            if (improved) continue;
            if (!pairs) break;
            // No single site helps: first strictly improving pair, in parallel.
            std::vector<std::vector<ExprPtr>> classes;
            for (size_t k = 0; k < sites; ++k) classes.push_back(siteClasses(k));
            std::vector<std::pair<size_t, size_t>> pairList;
            for (size_t i = 0; i < sites; ++i) {
                for (size_t j = i + 1; j < sites; ++j) pairList.emplace_back(i, j);
            }
            std::atomic<bool> found{false};
            std::atomic<size_t> nextPair{0};
            std::mutex m;
            Program bestProg;
            Score bestScore = cur;
            std::vector<std::thread> pool;
            for (unsigned t = 0; t < threads; ++t) {
                pool.emplace_back([&] {
                    for (size_t q; !found && (q = nextPair++) < pairList.size();) {
                        auto [i, j] = pairList[q];
                        for (const auto& fi : classes[i]) {
                            if (found) break;
                            Program pi = withHole(*w, i, fi.get());
                            for (const auto& fj : classes[j]) {
                                Program c = withHole(*pi, j, fj.get());
                                Score s;
                                if (scoreOn(*p, *c, climbSet, s, &cur) && s.beats(cur)) {
                                    std::lock_guard<std::mutex> lock(m);
                                    if (s.beats(bestScore)) {
                                        bestScore = s;
                                        bestProg = std::move(c);
                                    }
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                });
            }
            for (auto& th : pool) th.join();
            if (!bestProg) break;
            w = std::move(bestProg);
            cur = bestScore;
            pairMoves++;
        }
        Score fin;
        scoreOn(*p, *w, train, fin);
        bool solved = fin.solved == static_cast<int>(train.size());
        full += solved;
        std::cout << "trial " << trial << ": " << start.solved << " -> " << fin.solved << " / " << train.size()
                  << " (pair moves " << pairMoves << ")" << (solved ? "  SOLVED" : "") << "\n";
        if (solved) std::cout << w->toString() << "\n";
    }
    std::cout << "fully solved " << full << "/20" << (pairs ? " (with pairs)" : "") << "\n";
}
