#include <cstring>
#include <iostream>
#include <string>

#include "problems/SortingProblem.hpp"
#include "search/BeamSearch.hpp"
#include "search/Persistence.hpp"

using namespace algosearch;

namespace {

struct CliArgs {
    std::string problem = "sorting";
    int maxN = 5;
    int beamWidth = 500;
    int maxProgramSize = 30;
    long stepLimit = 5000;
    uint64_t seed = 1234;
    int startLevel = 1;
    int candidatesPerSize = 800;
    int numVars = 4;
    std::string output = "solutions.json";
};

bool parseArgs(int argc, char** argv, CliArgs& args) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto next = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << flag << "\n";
                std::exit(1);
            }
            return argv[++i];
        };
        if (arg == "--problem") args.problem = next("--problem");
        else if (arg == "--max-n") args.maxN = std::stoi(next("--max-n"));
        else if (arg == "--beam-width") args.beamWidth = std::stoi(next("--beam-width"));
        else if (arg == "--max-program-size") args.maxProgramSize = std::stoi(next("--max-program-size"));
        else if (arg == "--step-limit") args.stepLimit = std::stol(next("--step-limit"));
        else if (arg == "--seed") args.seed = static_cast<uint64_t>(std::stoull(next("--seed")));
        else if (arg == "--start-level") args.startLevel = std::stoi(next("--start-level"));
        else if (arg == "--candidates") args.candidatesPerSize = std::stoi(next("--candidates"));
        else if (arg == "--num-vars") args.numVars = std::stoi(next("--num-vars"));
        else if (arg == "--output") args.output = next("--output");
        else if (arg == "--help" || arg == "-h") return false;
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}

void printUsage() {
    std::cout << "algorithm-discovery --problem sorting --max-n 5 --beam-width 1000 "
                 "--max-program-size 30 --step-limit 10000 --seed 1234 "
                 "[--start-level 1] [--candidates 800] [--num-vars 4] [--output solutions.json]\n";
}

}  // namespace

int main(int argc, char** argv) {
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage();
        return 1;
    }

    if (args.problem != "sorting") {
        std::cerr << "Unknown problem: " << args.problem << " (only 'sorting' is available in V1)\n";
        return 1;
    }

    SortingProblem problem(/*exhaustiveMaxN=*/6, /*randomInstancesPerLevel=*/40, args.numVars, args.seed);

    SearchConfig config;
    config.beamWidth = args.beamWidth;
    config.maxProgramSize = args.maxProgramSize;
    config.stepLimit = args.stepLimit;
    config.startLevel = args.startLevel;
    config.maxLevel = args.maxN;
    config.seed = args.seed;
    config.candidatesPerSize = args.candidatesPerSize;
    config.genConfig.numVars = args.numVars;

    BeamSearch search(problem, config);
    std::vector<SolutionRecord> solutions = search.run();

    if (solutions.empty()) {
        std::cout << "\nNo fully correct program found within the given size/level limits.\n";
    } else {
        saveSolutions(args.output, solutions);
        std::cout << "\nSaved " << solutions.size() << " solution record(s) to " << args.output << "\n";
    }

    return 0;
}
