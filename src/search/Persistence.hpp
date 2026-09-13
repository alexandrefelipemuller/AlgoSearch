#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ast/Stmt.hpp"

namespace algosearch {

// A recorded solution: the discovered program plus its generalization record.
struct SolutionRecord {
    std::string programText;
    int programSize = 0;
    int levelReached = 0;
    int nReached = 0;
    int score = 0;
    int totalTests = 0;
    std::string finalStatus;
    // Diagnostic-only: how this program fared on out-of-distribution
    // instances (duplicates/negatives/wide-range/sorted/reversed) never
    // used to select or promote it. -1/-1 means no adversarial instances
    // were available for this level.
    int adversarialPassed = -1;
    int adversarialTotal = -1;
    uint64_t seed = 0;
    // Free-form key/value search configuration, stored verbatim for repro.
    std::vector<std::pair<std::string, std::string>> config;
};

// Appends `records` as a JSON array to `path` (overwrites the file).
// Hand-rolled writer: the schema is simple enough that pulling in a JSON
// dependency isn't warranted.
void saveSolutions(const std::string& path, const std::vector<SolutionRecord>& records);

}  // namespace algosearch
