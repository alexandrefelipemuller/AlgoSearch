#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

#include "problems/AverageProblem.hpp"
#include "problems/ContainsOrderedProblem.hpp"
#include "problems/CountProblem.hpp"
#include "problems/FactorialProblem.hpp"
#include "problems/FibonacciProblem.hpp"
#include "problems/ContainsProblem.hpp"
#include "problems/ThreeSumProblem.hpp"
#include "problems/SingleNumberProblem.hpp"
#include "problems/GraphReachabilityProblem.hpp"
#include "problems/ShortestPathProblem.hpp"
#include "problems/TspProblem.hpp"
#include "problems/GcdProblem.hpp"
#include "problems/ModProblem.hpp"
#include "problems/IsPrimeProblem.hpp"
#include "problems/MaxArrayProblem.hpp"
#include "problems/MinArrayProblem.hpp"
#include "problems/PowerProblem.hpp"
#include "problems/ReverseProblem.hpp"
#include "problems/SortingProblem.hpp"
#include "problems/SumArrayProblem.hpp"
#include "search/BeamSearch.hpp"
#include "search/Benchmark.hpp"
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
    unsigned threads = 0;  // 0 = auto-detect
    int invarianceLookahead = 3;
    double diversityRate = 0.0;
    double crossoverRate = 0.0;
    double shapeNicheRate = 0.0;
    double annealingTemperature = 0.0;
    double behaviorNicheRate = 0.0;
    double scoreBandRate = 0.0;
    double speedBonusWeight = 0.0;
    double loopBias = 0.4;  // best value found by a 160-run sweep, see ProgramGenerator.hpp
    double indexIncrementBias = 0.0;
    double bareSwapPenalty = 0.0;
    double controlFlowBias = 0.0;
    double adaptiveSkeletonBiasRate = 0.0;
    std::string adaptiveBiasLog;
    std::string output = "solutions.json";
    bool benchmark = false;
    int benchmarkSeeds = 5;
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
        else if (arg == "--threads") args.threads = static_cast<unsigned>(std::stoul(next("--threads")));
        else if (arg == "--invariance-lookahead") args.invarianceLookahead = std::stoi(next("--invariance-lookahead"));
        else if (arg == "--diversity-rate") args.diversityRate = std::stod(next("--diversity-rate"));
        else if (arg == "--crossover-rate") args.crossoverRate = std::stod(next("--crossover-rate"));
        else if (arg == "--shape-niche-rate") args.shapeNicheRate = std::stod(next("--shape-niche-rate"));
        else if (arg == "--annealing-temperature") args.annealingTemperature = std::stod(next("--annealing-temperature"));
        else if (arg == "--behavior-niche-rate") args.behaviorNicheRate = std::stod(next("--behavior-niche-rate"));
        else if (arg == "--score-band-rate") args.scoreBandRate = std::stod(next("--score-band-rate"));
        else if (arg == "--speed-bonus-weight") args.speedBonusWeight = std::stod(next("--speed-bonus-weight"));
        else if (arg == "--loop-bias") args.loopBias = std::stod(next("--loop-bias"));
        else if (arg == "--index-increment-bias") args.indexIncrementBias = std::stod(next("--index-increment-bias"));
        else if (arg == "--bare-swap-penalty") args.bareSwapPenalty = std::stod(next("--bare-swap-penalty"));
        else if (arg == "--control-flow-bias") args.controlFlowBias = std::stod(next("--control-flow-bias"));
        else if (arg == "--adaptive-skeleton-bias-rate") args.adaptiveSkeletonBiasRate = std::stod(next("--adaptive-skeleton-bias-rate"));
        else if (arg == "--adaptive-bias-log") args.adaptiveBiasLog = next("--adaptive-bias-log");
        else if (arg == "--output") args.output = next("--output");
        else if (arg == "--benchmark") args.benchmark = true;
        else if (arg == "--benchmark-seeds") args.benchmarkSeeds = std::stoi(next("--benchmark-seeds"));
        else if (arg == "--help" || arg == "-h") return false;
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return false;
        }
    }
    return true;
}

void printUsage() {
    std::cout << "algorithm-discovery --problem sorting|min|max|sum|contains|contains_ordered|is_prime|gcd|factorial|fibonacci|count|average|reverse|power|three_sum|single_number|graph_reachability|shortest_path|tsp --max-n 5 --beam-width 1000 "
                 "--max-program-size 30 --step-limit 10000 --seed 1234 "
                 "[--start-level 1] [--candidates 800] [--num-vars 4] [--threads 0] "
                 "[--invariance-lookahead 3] [--diversity-rate 0.0] [--crossover-rate 0.0] [--shape-niche-rate 0.0] "
                 "[--annealing-temperature 0.0] [--behavior-niche-rate 0.0] [--score-band-rate 0.0] [--speed-bonus-weight 0.0] [--loop-bias 0.4] "
                 "[--index-increment-bias 0.0] [--bare-swap-penalty 0.0] [--control-flow-bias 0.0] "
                 "[--adaptive-skeleton-bias-rate 0.0] [--adaptive-bias-log path.csv] "
                 "[--output solutions.json] "
                 "[--benchmark] [--benchmark-seeds 5]\n";
}

std::unique_ptr<Problem> makeProblem(const std::string& name, int numVars, uint64_t seed) {
    if (name == "sorting") return std::make_unique<SortingProblem>(6, 40, numVars, seed);
    if (name == "min") return std::make_unique<MinArrayProblem>(6, 40, numVars, seed);
    if (name == "max") return std::make_unique<MaxArrayProblem>(6, 40, numVars, seed);
    if (name == "sum") return std::make_unique<SumArrayProblem>(6, 40, numVars, seed);
    if (name == "contains") return std::make_unique<ContainsProblem>(6, 40, numVars, seed);
    if (name == "contains_ordered") return std::make_unique<ContainsOrderedProblem>(6, 40, numVars, seed);
    if (name == "is_prime") return std::make_unique<IsPrimeProblem>(30, 40, numVars, seed);
    if (name == "gcd") return std::make_unique<GcdProblem>(12, 40, numVars, seed);
    if (name == "mod") return std::make_unique<ModProblem>(12, 40, numVars, seed);
    if (name == "factorial") return std::make_unique<FactorialProblem>(10, 40, numVars, seed);
    if (name == "fibonacci") return std::make_unique<FibonacciProblem>(20, 40, numVars, seed);
    if (name == "count") return std::make_unique<CountProblem>(6, 40, numVars, seed);
    if (name == "average") return std::make_unique<AverageProblem>(6, 40, numVars, seed);
    if (name == "reverse") return std::make_unique<ReverseProblem>(6, 40, numVars, seed);
    if (name == "power") return std::make_unique<PowerProblem>(6, 40, numVars, seed);
    if (name == "three_sum") return std::make_unique<ThreeSumProblem>(6, 40, numVars, seed);
    if (name == "single_number") return std::make_unique<SingleNumberProblem>(4, 40, numVars, seed);
    if (name == "graph_reachability") return std::make_unique<GraphReachabilityProblem>(6, 40, numVars, seed);
    if (name == "shortest_path") return std::make_unique<ShortestPathProblem>(6, 40, numVars, seed);
    if (name == "tsp") return std::make_unique<TspProblem>(6, 40, numVars, seed);
    return nullptr;
}

// Runs the search across `args.benchmarkSeeds` independent seeds, collects
// every distinct (by program text) SUCCESS-labeled program found across all
// of them, independently re-verifies generalization for each (never trusts
// the search's own SUCCESS label at face value -- see
// Benchmark::verifyGeneralization), discards anything that fails that
// check, times the survivors across a range of input sizes, fits an
// empirical time-complexity curve to each, and reports them ranked fastest
// first.
void runBenchmarkMode(const CliArgs& args) {
    struct Candidate {
        std::string programText;
        int programSize = 0;
        Program program;
    };
    std::unordered_map<std::string, Candidate> candidates;  // keyed by programText

    for (int i = 0; i < args.benchmarkSeeds; ++i) {
        uint64_t seed = args.seed + static_cast<uint64_t>(i);
        std::unique_ptr<Problem> problem = makeProblem(args.problem, args.numVars, seed);
        if (!problem) {
            std::cerr << "Unknown problem: " << args.problem << "\n";
            return;
        }

        SearchConfig config;
        config.beamWidth = args.beamWidth;
        config.maxProgramSize = args.maxProgramSize;
        config.stepLimit = args.stepLimit;
        config.startLevel = args.startLevel;
        config.maxLevel = args.maxN;
        config.seed = seed;
        config.candidatesPerSize = args.candidatesPerSize;
        config.genConfig.numVars = args.numVars;
        config.numThreads = args.threads;
        config.invarianceLookahead = args.invarianceLookahead;
        config.diversityRate = args.diversityRate;
        config.crossoverRate = args.crossoverRate;
        config.shapeNicheRate = args.shapeNicheRate;
        config.annealingTemperature = args.annealingTemperature;
        config.behaviorNicheRate = args.behaviorNicheRate;
        config.scoreBandRate = args.scoreBandRate;
        config.speedBonusWeight = args.speedBonusWeight;
        config.genConfig.loopAccumulatorBias = args.loopBias;
        config.genConfig.indexIncrementBias = args.indexIncrementBias;
        config.genConfig.bareSwapPenalty = args.bareSwapPenalty;
        config.genConfig.controlFlowBias = args.controlFlowBias;

        std::cout << "[benchmark] seed " << (i + 1) << "/" << args.benchmarkSeeds << " (" << seed << ")...\n";
        BeamSearch search(*problem, config);
        std::vector<Program> programs;
        std::vector<SolutionRecord> solutions = search.run(&programs);

        for (size_t r = 0; r < solutions.size(); ++r) {
            if (solutions[r].finalStatus != "SUCCESS") continue;
            const std::string& text = solutions[r].programText;
            if (candidates.count(text)) continue;
            Candidate c;
            c.programText = text;
            c.programSize = solutions[r].programSize;
            c.program = std::move(programs[r]);
            candidates.emplace(text, std::move(c));
        }
    }

    std::cout << "\n[benchmark] " << candidates.size() << " distinct SUCCESS-labeled program(s) found across "
              << args.benchmarkSeeds << " seed(s). Independently re-verifying generalization...\n";

    std::unique_ptr<Problem> problemForEval = makeProblem(args.problem, args.numVars, args.seed);
    std::vector<int> sizes = {10, 20, 40, 80, 160, 320, 640, 1280};
    // Deliberately NOT args.stepLimit: that value is tuned to keep the
    // SEARCH cheap (abort runaway candidates fast), but would silently
    // truncate a genuinely correct program's execution at large benchmark
    // sizes, making it look artificially fast and corrupting the fitted
    // complexity exponent. Benchmarking needs a limit high enough to never
    // be the reason a correct program stops early.
    const long kBenchmarkStepLimit = 50'000'000;

    std::vector<BenchmarkResult> results;
    int rejected = 0;
    for (auto& [text, c] : candidates) {
        BenchmarkResult res;
        res.programText = c.programText;
        res.programSize = c.programSize;
        std::string reason;
        res.generalizes = verifyGeneralization(*problemForEval, *c.program, kBenchmarkStepLimit, reason);
        if (!res.generalizes) {
            res.failureReason = reason;
            rejected++;
            std::cout << "  REJECTED (does not generalize): " << reason << "\n";
            continue;
        }
        res.timings = timeProgram(*problemForEval, *c.program, kBenchmarkStepLimit, sizes);
        auto [exponent, r2] = fitPowerLaw(res.timings);
        res.exponent = exponent;
        res.r2 = r2;
        res.complexityLabel = classifyExponent(exponent);
        results.push_back(std::move(res));
    }

    std::cout << "\n[benchmark] " << results.size() << " genuinely generalizing solution(s) kept, " << rejected
              << " rejected.\n\n";

    if (results.empty()) {
        std::cout << "No generalizing solution survived independent re-verification.\n";
        return;
    }

    std::sort(results.begin(), results.end(), [](const BenchmarkResult& a, const BenchmarkResult& b) {
        double ta = a.timings.empty() ? 1e300 : a.timings.back().avgNanos;
        double tb = b.timings.empty() ? 1e300 : b.timings.back().avgNanos;
        return ta < tb;
    });

    std::cout << std::left << std::setw(4) << "#" << std::setw(8) << "size" << std::setw(16) << "time@maxN(us)"
              << std::setw(10) << "exponent" << std::setw(8) << "r2" << "complexity\n";
    for (size_t i = 0; i < results.size(); ++i) {
        const auto& r = results[i];
        double timeUs = r.timings.empty() ? 0.0 : r.timings.back().avgNanos / 1000.0;
        std::cout << std::left << std::setw(4) << (i == 0 ? "*" : std::to_string(i + 1)) << std::setw(8) << r.programSize
                  << std::setw(16) << std::fixed << std::setprecision(3) << timeUs << std::setw(10) << std::setprecision(2)
                  << r.exponent << std::setw(8) << r.r2 << r.complexityLabel << "\n";
    }
    std::cout << "\n(* = fastest at the largest benchmarked size)\n\n";
    std::cout << "=== fastest solution's program ===\n" << results.front().programText << "\n";
}

}  // namespace

int main(int argc, char** argv) {
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage();
        return 1;
    }

    std::unique_ptr<Problem> problem = makeProblem(args.problem, args.numVars, args.seed);
    if (!problem) {
        std::cerr << "Unknown problem: " << args.problem
                   << " (available: sorting, min, max, sum, contains, contains_ordered, is_prime, gcd, factorial, fibonacci, "
                      "count, average, reverse, power)\n";
        return 1;
    }

    if (args.benchmark) {
        runBenchmarkMode(args);
        return 0;
    }

    SearchConfig config;
    config.beamWidth = args.beamWidth;
    config.maxProgramSize = args.maxProgramSize;
    config.stepLimit = args.stepLimit;
    config.startLevel = args.startLevel;
    config.maxLevel = args.maxN;
    config.seed = args.seed;
    config.candidatesPerSize = args.candidatesPerSize;
    config.genConfig.numVars = args.numVars;
    config.numThreads = args.threads;
    config.invarianceLookahead = args.invarianceLookahead;
    config.diversityRate = args.diversityRate;
    config.crossoverRate = args.crossoverRate;
    config.shapeNicheRate = args.shapeNicheRate;
    config.annealingTemperature = args.annealingTemperature;
    config.behaviorNicheRate = args.behaviorNicheRate;
    config.scoreBandRate = args.scoreBandRate;
    config.speedBonusWeight = args.speedBonusWeight;
    config.genConfig.loopAccumulatorBias = args.loopBias;
    config.genConfig.indexIncrementBias = args.indexIncrementBias;
    config.genConfig.bareSwapPenalty = args.bareSwapPenalty;
    config.genConfig.controlFlowBias = args.controlFlowBias;
    config.genConfig.adaptiveSkeletonBiasRate = args.adaptiveSkeletonBiasRate;
    config.adaptiveBiasLogPath = args.adaptiveBiasLog;

    BeamSearch search(*problem, config);
    std::vector<SolutionRecord> solutions = search.run();

    if (solutions.empty()) {
        std::cout << "\nNo fully correct program found within the given size/level limits.\n";
    } else {
        saveSolutions(args.output, solutions);
        std::cout << "\nSaved " << solutions.size() << " solution record(s) to " << args.output << "\n";
    }

    return 0;
}
