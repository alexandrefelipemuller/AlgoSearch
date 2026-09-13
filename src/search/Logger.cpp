#include "search/Logger.hpp"

#include <iostream>

namespace algosearch {
namespace log {

void levelHeader(int level, const std::string& levelDescription, size_t numInstances, int beamWidth) {
    std::cout << "\nLEVEL " << level << "\n"
              << levelDescription << "\n"
              << "instances = " << numInstances << "\n"
              << "beam = " << beamWidth << "\n";
}

void progress(size_t numCandidates, int bestScore, int totalTests, int bestProgramSize, int timeouts, int errors) {
    std::cout << "  candidates = " << numCandidates << " | best score = " << bestScore << "/" << totalTests
              << " | best size = " << bestProgramSize << " | timeouts = " << timeouts << " | errors = " << errors
              << "\n";
}

void solutionFound(int programSize, const std::string& programText) {
    std::cout << "\nSOLUTION FOUND\n"
              << "program size = " << programSize << "\n\n"
              << programText << "\n";
}

void advancingToLevel(int level) { std::cout << "ADVANCING TO LEVEL " << level << "\n"; }

void searchFinished(double totalSeconds) { std::cout << "\nSearch finished in " << totalSeconds << "s\n"; }

void info(const std::string& msg) { std::cout << msg << "\n"; }

}  // namespace log
}  // namespace algosearch
