// Section 66 pilot: hill-climb scrambled skeletons site by site (full class
// lists, no cap) on a problem's training levels; report final scores.
#include <algorithm>
#include <iostream>
#include <memory>
#include "interpreter/Interpreter.hpp"
#include "problems/GcdProblem.hpp"
#include "problems/FactorialProblem.hpp"
#include "search/HoleGrammar.hpp"
#include "search/ProgramGenerator.hpp"
using namespace algosearch;
int main(int argc, char** argv) {
    std::string which = argc > 1 ? argv[1] : "gcd";
    std::unique_ptr<Problem> p;
    std::string sk;
    if (which == "gcd") { p = std::make_unique<GcdProblem>(20, 40, 8); sk = "recurrence"; }
    else { p = std::make_unique<FactorialProblem>(15, 40, 8); sk = "repeatedmultiply"; }
    std::vector<Instance> train;
    for (int l = 1; l <= 6; l = p->nextLevel(l)) for (auto& i : p->generateInstances(l)) train.push_back(i);
    setenv("ALGOSEARCH_FORCE_SKELETON", sk.c_str(), 1);
    GenConfig cfg; cfg.numVars = 8; cfg.loopAccumulatorBias = 1.0; cfg.combinatorBias = 0.0; cfg.skeletonHoles = true;
    Interpreter in(20000);
    std::vector<int> out;
    auto score = [&](const Stmt& pr, int& s, long& st) { s = 0; st = 0; for (auto& i : train) { long x = 0; if (p->evaluateInto(pr, i, in, out, &x) == ExecStatus::SUCCESS) s++; st += x; } };
    int full = 0;
    for (int trial = 0; trial < 20; ++trial) {
        Random rng(100 + trial);
        Program w = generateRandomProgram(rng, 2, cfg);
        int cs; long ct; score(*w, cs, ct);
        int start = cs;
        size_t sites = 0; forEachSlot(*w, [&](Stmt&, ExprPtr&, bool) { sites++; });
        std::set<int> vs = {0}; collectVars(*w, vs); std::vector<int> vars(vs.begin(), vs.end());
        for (int pass = 0; pass < 6; ++pass) {
            bool imp = false;
            for (size_t k = 0; k < sites; ++k) {
                bool isCond = false; size_t idx = 0;
                forEachSlot(*w, [&](Stmt&, ExprPtr&, bool c) { if (idx++ == k) isCond = c; });
                std::vector<HoleState> states;
                const Stmt* site = nullptr; Program tr = withHole(*w, k, nullptr, &site);
                Interpreter t2(20000); t2.setConditionTrace(site, [&](const std::vector<int>& v, const std::vector<int>& a) { if (states.size() < 300) states.push_back({v, a}); });
                for (auto& i : train) t2.run(*tr, i.input, 8, out);
                auto cls = dedupeOnStates(isCond ? enumerateConds(vars) : enumerateValues(vars), states);
                for (auto& f : cls) { Program c = withHole(*w, k, f.get()); int s; long st; score(*c, s, st);
                    if (s > cs || (s == cs && st < ct)) { cs = s; ct = st; w = std::move(c); imp = true; } }
            }
            if (!imp) break;
        }
        full += cs == (int)train.size();
        std::cout << "trial " << trial << ": " << start << " -> " << cs << " / " << train.size() << "\n";
    }
    std::cout << "fully solved " << full << "/20\n";
}
