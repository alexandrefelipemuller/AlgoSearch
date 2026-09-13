#pragma once

#include <cstdint>
#include <vector>

#include "problems/Problem.hpp"
#include "search/Persistence.hpp"
#include "search/ProgramGenerator.hpp"

namespace algosearch {

struct SearchConfig {
    int beamWidth = 200;
    int maxProgramSize = 30;
    long stepLimit = 10000;
    int startLevel = 1;
    int maxLevel = 5;
    uint64_t seed = 1234;
    int candidatesPerSize = 500;  // how many candidates to generate/score per program size
    GenConfig genConfig;
    unsigned numThreads = 0;  // 0 = auto-detect (std::thread::hardware_concurrency())
};

// Beam Search over the space of Mini-C programs for a given Problem.
//
// Grows programs from very small ASTs up to `maxProgramSize`, scoring each
// by the number of correctly solved instances of the current problem level
// (score = correctness count only, never mixed with program size). Whenever
// a candidate fully solves a level, the problem is promoted to harder
// levels immediately, using the winning program as long as it keeps solving
// each next level completely.
class BeamSearch {
public:
    BeamSearch(const Problem& problem, SearchConfig config);

    // Runs the search; returns every solution found (one per level fully
    // reached and never re-broken), most recent last.
    std::vector<SolutionRecord> run();

private:
    const Problem& problem_;
    SearchConfig config_;
};

}  // namespace algosearch
