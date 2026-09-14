#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ast/Stmt.hpp"

namespace algosearch {

// A recorded solution: the discovered program plus its generalization record.
struct SolutionRecord {
    // Stable id (derived from the program's canonical text) identifying
    // this exact program across the multiple per-N rows a single cascade
    // produces -- e.g. every row from a program found at N=2 that keeps
    // working through N=8 shares one programId.
    std::string programId;
    std::string programText;
    int programSize = 0;
    // The N this specific row reports on (may differ from firstLevel: a
    // program found at N=2 that survives to N=5 produces one row per N in
    // [2,5], each with this field set to that N).
    int levelReached = 0;
    int nReached = 0;
    // The N where this program was ORIGINALLY discovered by the search
    // (constant across every row sharing this programId).
    int firstLevel = 0;
    // Generalization horizon: how many consecutive levels, starting at
    // firstLevel, this exact unmutated program was verified to fully
    // solve (constant across every row sharing this programId). A program
    // found at N=2 that still works at N=8 with maxLevel=8 has
    // confirmedLevels=7; one that breaks at N=3 has confirmedLevels=1.
    int confirmedLevels = 0;
    int score = 0;
    int totalTests = 0;
    std::string finalStatus;
    // Diagnostic-only: how this program fared on out-of-distribution
    // instances (duplicates/negatives/wide-range/sorted/reversed) never
    // used to select or promote it. -1/-1 means no adversarial instances
    // were available for this level.
    int adversarialPassed = -1;
    int adversarialTotal = -1;
    // Diagnostic-only: structural diversity of the beam at the moment this
    // program's full solve was detected -- the number of distinct coarse
    // "shapes" (counts of assign/if/while/swap/return + max loop nesting,
    // ignoring exact literals/variable indices) among the beamWidth
    // survivors, out of beamSize total. A small count relative to beamSize
    // is evidence of premature convergence onto near-identical variants of
    // one solution; -1/-1 if unavailable.
    int beamDiversity = -1;
    int beamSize = -1;
    uint64_t seed = 0;
    // Free-form key/value search configuration, stored verbatim for repro.
    std::vector<std::pair<std::string, std::string>> config;
};

// Appends `records` as a JSON array to `path` (overwrites the file).
// Hand-rolled writer: the schema is simple enough that pulling in a JSON
// dependency isn't warranted.
void saveSolutions(const std::string& path, const std::vector<SolutionRecord>& records);

}  // namespace algosearch
