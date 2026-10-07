// FINDINGS.md section 70: phase-1 reachability for the five problems no arm
// has ever solved. Samples programs from the DEFAULT generator family (CLI
// defaults: combinator 0.5 with step axes and stateful pieces, loop bias 0.4,
// every remaining skeleton) and asks whether any is correct on the training
// levels -- i.e. whether the family contains a solution at all, before any
// question of search. Three shapes per sample:
//   one   : a single drawn top-level block
//   one+T : that block followed by section 51's enumerated tail
//   two+T : two independently drawn blocks in sequence, then the tail
// For each shape it reports how many samples are fully correct on training,
// how many of those also pass held-out levels, and the best near miss.
//
// Build (after `make`):
//   g++ -std=c++20 -O2 -Isrc scripts/probe_reachability.cpp \
//       $(find build/lib -name '*.o' ! -name main.o) -o probe_reachability -lpthread
// Usage: ./probe_reachability <problem> [samples=20000] [seed=1]

#include <iostream>
#include <memory>
#include <string>

#include "interpreter/Interpreter.hpp"
#include "problems/CountLeadersProblem.hpp"
#include "problems/CycleDetectionProblem.hpp"
#include "problems/HasMajorityProblem.hpp"
#include "problems/MaxAdjacentGapProblem.hpp"
#include "problems/MaxProfitProblem.hpp"
#include "search/ProgramGenerator.hpp"
#include "search/TailFiller.hpp"

using namespace algosearch;

int main(int argc, char** argv) {
    std::string name = argc > 1 ? argv[1] : "max_profit";
    int samples = argc > 2 ? std::stoi(argv[2]) : 20000;
    uint64_t seed = argc > 3 ? std::stoull(argv[3]) : 1;
    const int numVars = 9;
    std::unique_ptr<Problem> p;
    int trainMax = 8;
    if (name == "max_adjacent_gap") p = std::make_unique<MaxAdjacentGapProblem>(numVars);
    else if (name == "max_profit") p = std::make_unique<MaxProfitProblem>(numVars);
    else if (name == "count_leaders") p = std::make_unique<CountLeadersProblem>(numVars);
    else if (name == "has_majority") p = std::make_unique<HasMajorityProblem>(numVars);
    else if (name == "cycle") {
        p = std::make_unique<CycleDetectionProblem>(6, 30, numVars);
        trainMax = 5;
    } else {
        std::cerr << "unknown problem\n";
        return 1;
    }
    std::vector<Instance> train, heldOut;
    int level = 1;
    for (; level <= trainMax; level = p->nextLevel(level)) {
        for (auto& i : p->generateInstances(level)) train.push_back(i);
    }
    for (int k = 0; k < 3; ++k, level = p->nextLevel(level)) {
        for (auto& i : p->generateInstances(level)) heldOut.push_back(i);
        for (auto& i : p->generateAdversarialInstances(level)) heldOut.push_back(i);
    }

    // CLI defaults (src/main.cpp), as the search sees them.
    GenConfig cfg;
    cfg.numVars = numVars;
    cfg.combinatorBias = 0.5;
    cfg.combinatorStep = true;
    cfg.combinatorState = true;
    Interpreter interp(20000);
    std::vector<int> out;
    auto solved = [&](const Stmt& prog, const std::vector<Instance>& insts) {
        int ok = 0;
        for (const auto& i : insts) ok += p->evaluateInto(prog, i, interp, out) == ExecStatus::SUCCESS;
        return ok;
    };

    struct Shape {
        const char* label;
        int correct = 0, generalizes = 0, best = -1;
        std::string bestText;
    } shapes[3] = {{"one"}, {"one+T"}, {"two+T"}};
    auto record = [&](Shape& s, const Stmt& prog) {
        int ok = solved(prog, train);
        if (ok == static_cast<int>(train.size())) {
            s.correct++;
            if (solved(prog, heldOut) == static_cast<int>(heldOut.size())) {
                if (s.generalizes++ == 0) s.bestText = prog.toString();
            }
        }
        if (ok > s.best) {
            s.best = ok;
            if (s.generalizes == 0) s.bestText = prog.toString();
        }
    };

    Random rng(seed);
    for (int i = 0; i < samples; ++i) {
        Program a = generateRandomProgram(rng, 2, cfg);
        record(shapes[0], *a);
        if (auto t = fillTail(*a, train, *p, interp)) record(shapes[1], *t);
        Program b = generateRandomProgram(rng, 2, cfg);
        auto both = makeBlock();
        for (auto& s : a->body) both->body.push_back(s->clone());
        for (auto& s : b->body) both->body.push_back(s->clone());
        if (auto t = fillTail(*both, train, *p, interp)) record(shapes[2], *t);
    }
    std::cout << "[reach] " << name << ": " << samples << " samples, " << train.size() << " training / " << heldOut.size()
              << " held-out instances\n";
    for (const auto& s : shapes) {
        std::cout << "[reach] " << s.label << ": correct on training " << s.correct << ", also held-out " << s.generalizes
                  << ", best " << s.best << "/" << train.size() << "\n";
    }
    for (const auto& s : shapes) {
        std::cout << "--- " << s.label << (s.generalizes ? " (a generalizing solution)" : " (best near miss)") << ":\n"
                  << s.bestText << "\n";
    }
}
