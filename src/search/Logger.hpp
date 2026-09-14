#pragma once

#include <string>

namespace algosearch {

// Minimal stdout logger producing the human-readable progress format
// described in the project spec.
namespace log {

void levelHeader(int level, const std::string& levelDescription, size_t numInstances, int beamWidth);
void progress(size_t numCandidates, int bestScore, int totalTests, int bestProgramSize, int timeouts, int errors);
void advancingToLevel(int level);
void searchFinished(double totalSeconds);
void info(const std::string& msg);

// One champion record per promoted level: "N=<level> / score X/Y / size S
// / held-out A/B" followed by the program text. Emitted once per level
// actually reached (not just the final one in a promotion cascade), so
// consecutive N's champions can be diffed by eye to tell a generalizing
// algorithm (identical text across N) from an N-specific fit (new, bigger
// text at each N). advPassed/advTotal may be -1 if no held-out instances
// were available for that level.
void championAtLevel(int level, int score, int total, int programSize, const std::string& programText, int advPassed,
                      int advTotal, const std::string& programId, int firstLevel, int confirmedLevels);

}  // namespace log
}  // namespace algosearch
