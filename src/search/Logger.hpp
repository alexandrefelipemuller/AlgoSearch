#pragma once

#include <string>

namespace algosearch {

// Minimal stdout logger producing the human-readable progress format
// described in the project spec.
namespace log {

void levelHeader(int level, const std::string& levelDescription, size_t numInstances, int beamWidth);
void progress(size_t numCandidates, int bestScore, int totalTests, int bestProgramSize, int timeouts, int errors);
void solutionFound(int programSize, const std::string& programText);
void advancingToLevel(int level);
void searchFinished(double totalSeconds);
void info(const std::string& msg);

}  // namespace log
}  // namespace algosearch
