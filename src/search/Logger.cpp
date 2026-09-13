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

void advancingToLevel(int level) { std::cout << "ADVANCING TO LEVEL " << level << "\n"; }

void searchFinished(double totalSeconds) { std::cout << "\nSearch finished in " << totalSeconds << "s\n"; }

void info(const std::string& msg) { std::cout << msg << "\n"; }

void championAtLevel(int level, int score, int total, int programSize, const std::string& programText, int advPassed,
                      int advTotal) {
    std::cout << "\nN=" << level << "\n"
              << "  score " << score << "/" << total << "\n"
              << "  size " << programSize;
    if (advTotal >= 0) {
        std::cout << "  held-out " << advPassed << "/" << advTotal;
    }
    std::cout << "\n  programa:\n" << programText << "\n";
}

}  // namespace log
}  // namespace algosearch
