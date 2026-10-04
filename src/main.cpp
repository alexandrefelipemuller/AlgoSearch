#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

#include "problems/AverageProblem.hpp"
#include "problems/ContainsOrderedProblem.hpp"
#include "problems/CountProblem.hpp"
#include "problems/FactorialProblem.hpp"
#include "problems/FibonacciProblem.hpp"
#include "problems/ContainsProblem.hpp"
#include "problems/ThreeSumProblem.hpp"
#include "problems/SingleNumberProblem.hpp"
#include "problems/CycleDetectionProblem.hpp"
#include "problems/DominatedWindowProblem.hpp"
#include "problems/CountDescentsProblem.hpp"
#include "problems/StrictlyIncreasingProblem.hpp"
#include "problems/HasAdjacentEqualProblem.hpp"
#include "problems/LongestRunProblem.hpp"
#include "problems/MedianProblem.hpp"
#include "problems/SecondSmallestProblem.hpp"
#include "problems/CountDistinctProblem.hpp"
#include "problems/MinGapProblem.hpp"
#include "problems/MaxGapSortedProblem.hpp"
#include "problems/MaxSubarraySumProblem.hpp"
#include "problems/LongestIncreasingRunProblem.hpp"
#include "problems/MaxProfitProblem.hpp"
#include "problems/CountLeadersProblem.hpp"
#include "problems/HasMajorityProblem.hpp"
#include "problems/TrappingRainWaterProblem.hpp"
#include "problems/MaxAdjacentGapProblem.hpp"
#include "problems/HasDuplicateProblem.hpp"
#include "problems/InversionCountProblem.hpp"
#include "problems/IsSortedProblem.hpp"
#include "problems/PalindromeProblem.hpp"
#include "problems/DtwProblem.hpp"
#include "problems/LisProblem.hpp"
#include "problems/SubstringProblem.hpp"
#include "problems/GraphReachabilityProblem.hpp"
#include "problems/ShortestPathProblem.hpp"
#include "problems/TspProblem.hpp"
#include "search/HoleFiller.hpp"
#include "search/ProgramGenerator.hpp"
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
    bool numVarsExplicit = false;  // true once --num-vars is parsed; see resolveNumVars
    unsigned threads = 0;  // 0 = auto-detect
    int invarianceLookahead = 3;
    double diversityRate = 0.0;
    double crossoverRate = 0.0;
    double shapeNicheRate = 0.0;
    double annealingTemperature = 0.0;
    double behaviorNicheRate = 0.0;
    double scoreBandRate = 0.0;
    double speedBonusWeight = 0.0;
    int postSolveSizes = 0;
    int tailEnumTop = 0;
    double loopBias = 0.4;  // best value found by a 160-run sweep, see ProgramGenerator.hpp
    double indexIncrementBias = 0.0;
    double bareSwapPenalty = 0.0;
    double controlFlowBias = 0.0;
    double compositeBias = 0.0;
    double exprMutationRate = 0.0;
    bool pruneGeneration = false;
    bool roleTypedVars = false;
    double combinatorBias = 0.5;  // default since FINDINGS.md section 48 (A/B in section 47); --combinator-bias 0 restores the old default
    bool combinatorAdjacent = true;
    bool combinatorStep = false;  // v2 axes by default; --combinator-v1 restores the frozen v1 family
    double adaptiveSkeletonBiasRate = 0.0;
    double secondArrayBias = 0.0;
    double recursionCallBias = 0.0;
    std::string adaptiveBiasLog;
    std::string output = "solutions.json";
    std::string checkpointPath;
    bool benchmark = false;
    bool fillTspGuard = false;
    bool soundnessLabels = false;
    bool metamorphic = false;
    bool rankWorstScale = false;
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
        else if (arg == "--num-vars") { args.numVars = std::stoi(next("--num-vars")); args.numVarsExplicit = true; }
        else if (arg == "--threads") args.threads = static_cast<unsigned>(std::stoul(next("--threads")));
        else if (arg == "--invariance-lookahead") args.invarianceLookahead = std::stoi(next("--invariance-lookahead"));
        else if (arg == "--diversity-rate") args.diversityRate = std::stod(next("--diversity-rate"));
        else if (arg == "--crossover-rate") args.crossoverRate = std::stod(next("--crossover-rate"));
        else if (arg == "--shape-niche-rate") args.shapeNicheRate = std::stod(next("--shape-niche-rate"));
        else if (arg == "--annealing-temperature") args.annealingTemperature = std::stod(next("--annealing-temperature"));
        else if (arg == "--behavior-niche-rate") args.behaviorNicheRate = std::stod(next("--behavior-niche-rate"));
        else if (arg == "--score-band-rate") args.scoreBandRate = std::stod(next("--score-band-rate"));
        else if (arg == "--speed-bonus-weight") args.speedBonusWeight = std::stod(next("--speed-bonus-weight"));
        else if (arg == "--post-solve-sizes") args.postSolveSizes = std::stoi(next("--post-solve-sizes"));
        else if (arg == "--tail-enum") args.tailEnumTop = std::stoi(next("--tail-enum"));
        else if (arg == "--loop-bias") args.loopBias = std::stod(next("--loop-bias"));
        else if (arg == "--index-increment-bias") args.indexIncrementBias = std::stod(next("--index-increment-bias"));
        else if (arg == "--bare-swap-penalty") args.bareSwapPenalty = std::stod(next("--bare-swap-penalty"));
        else if (arg == "--control-flow-bias") args.controlFlowBias = std::stod(next("--control-flow-bias"));
        else if (arg == "--composite-bias") args.compositeBias = std::stod(next("--composite-bias"));
        else if (arg == "--expr-mutation-rate") args.exprMutationRate = std::stod(next("--expr-mutation-rate"));
        else if (arg == "--prune-generation") args.pruneGeneration = true;
        else if (arg == "--role-typed-vars") args.roleTypedVars = true;
        else if (arg == "--combinator-bias") args.combinatorBias = std::stod(next("--combinator-bias"));
        else if (arg == "--combinator-adjacent") args.combinatorAdjacent = true;  // now the default; kept so section 40's scripts still run
        else if (arg == "--combinator-v1") args.combinatorAdjacent = false;
        else if (arg == "--combinator-step") args.combinatorStep = true;
        else if (arg == "--adaptive-skeleton-bias-rate") args.adaptiveSkeletonBiasRate = std::stod(next("--adaptive-skeleton-bias-rate"));
        else if (arg == "--second-array-bias") args.secondArrayBias = std::stod(next("--second-array-bias"));
        else if (arg == "--recursion-call-bias") args.recursionCallBias = std::stod(next("--recursion-call-bias"));
        else if (arg == "--adaptive-bias-log") args.adaptiveBiasLog = next("--adaptive-bias-log");
        else if (arg == "--output") args.output = next("--output");
        else if (arg == "--checkpoint-path") args.checkpointPath = next("--checkpoint-path");
        else if (arg == "--benchmark") args.benchmark = true;
        else if (arg == "--fill-tsp-guard") args.fillTspGuard = true;
        else if (arg == "--soundness-labels") args.soundnessLabels = true;
        else if (arg == "--metamorphic") args.metamorphic = true;
        else if (arg == "--rank-worst-scale") args.rankWorstScale = true;
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
    std::cout << "algorithm-discovery --problem sorting|min|max|sum|contains|contains_ordered|is_prime|gcd|factorial|fibonacci|count|average|reverse|power|three_sum|single_number|graph_reachability|shortest_path|tsp|dtw|lis|substring|cycle|dominated_window|palindrome|inversion_count|has_duplicate|is_sorted|count_descents|strictly_increasing|has_adjacent_equal|longest_run|max_adjacent_gap|median|second_smallest|count_distinct|min_gap|max_gap_sorted|max_subarray_sum|longest_increasing_run|max_profit|count_leaders|has_majority|trapping_rain_water --max-n 5 --beam-width 1000 "
                 "--max-program-size 30 --step-limit 10000 --seed 1234 "
                 "[--start-level 1] [--candidates 800] [--num-vars 4 (auto-raised per-problem if omitted; see resolveNumVars)] [--threads 0] "
                 "[--invariance-lookahead 3] [--diversity-rate 0.0] [--crossover-rate 0.0] [--shape-niche-rate 0.0] "
                 "[--annealing-temperature 0.0] [--behavior-niche-rate 0.0] [--score-band-rate 0.0] [--speed-bonus-weight 0.0] [--post-solve-sizes 0] [--tail-enum 0] [--loop-bias 0.4] "
                 "[--index-increment-bias 0.0] [--bare-swap-penalty 0.0] [--control-flow-bias 0.0] [--composite-bias 0.0] [--expr-mutation-rate 0.0] [--prune-generation] [--role-typed-vars] [--combinator-bias 0.5] [--combinator-v1] [--combinator-step] "
                 "[--adaptive-skeleton-bias-rate 0.0] [--adaptive-bias-log path.csv] "
                 "[--second-array-bias 0.0] [--recursion-call-bias 0.0] "
                 "[--output solutions.json] [--checkpoint-path path.json] "
                 "[--benchmark] [--benchmark-seeds 5] [--fill-tsp-guard [--soundness-labels] [--metamorphic [--rank-worst-scale]]]\n";
}

// The dedicated skeletons in ProgramGenerator.cpp gate themselves on
// GenConfig::numVars thresholds (e.g. "cfg.numVars >= 12" for tsp/dtw) that
// have nothing to do with problem difficulty and everything to do with how
// many scratch scalars that skeleton's hand-written shape needs -- pass too
// few and the skeleton is silently never offered, so the search falls back
// to free/generic search and burns its whole budget never finding it (see
// FINDINGS.md sections 18-19 for what that failure mode looks like). This
// table mirrors those gates so a caller who doesn't pass --num-vars gets a
// value that actually lets the relevant skeleton fire, instead of the
// generic default of 4.
int recommendedNumVars(const std::string& problem) {
    if (problem == "tsp" || problem == "dtw") return 12;           // TspBruteForce/TspBranchAndBound/Dtw skeletons
    if (problem == "shortest_path") return 7;                       // ShortestPath skeleton
    if (problem == "graph_reachability") return 6;                  // GraphReachability skeleton
    if (problem == "three_sum") return 5;                           // ThreeSum skeleton
    if (problem == "average") return 5;                             // Average skeleton
    return 4;
}

// Resolves the numVars to actually search with: always the larger of what
// was passed (or the CliArgs default, if --num-vars was never passed) and
// the problem's own skeleton requirement, so a value that's too low to
// reach the tuned skeleton can never silently waste the whole search
// budget on free/generic search (see FINDINGS.md sections 18-19 for what
// that failure mode looks like). There is no free-search-only mode via
// --num-vars any more -- use ALGOSEARCH_DISABLE_SKELETON for that
// (see FINDINGS.md section 18).
void resolveNumVars(CliArgs& args) {
    int recommended = recommendedNumVars(args.problem);
    // --composite-bias > 0 can trigger genCompositeBlock regardless of
    // problem, which needs numVars >= 8 (pickDistinctVars draws 6 distinct
    // scratch vars from a pool of numVars-1) -- unlike an under-provisioned
    // skeleton gate (which just never fires, wasting budget), an
    // under-provisioned pickDistinctVars pool never terminates. Learned the
    // hard way: numVars=6 here hung a test for hours before this and the
    // matching ProgramGenerator.cpp gate fix (FINDINGS.md section 32).
    if (args.compositeBias > 0.0 && recommended < 8) recommended = 8;
    if (args.combinatorBias > 0.0 && recommended < 8) recommended = 8;  // same 6-var binding context
    if (args.numVars < recommended) {
        if (args.numVarsExplicit) {
            std::cerr << "note: raising --num-vars from " << args.numVars << " to " << recommended
                       << " -- problem '" << args.problem << "' needs at least that many for its dedicated "
                          "skeleton to fire (see FINDINGS.md sections 18-19). Use "
                          "ALGOSEARCH_DISABLE_SKELETON to force free search instead.\n";
        }
        args.numVars = recommended;
    }
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
    if (name == "dtw") return std::make_unique<DtwProblem>(5, 40, numVars, seed);
    if (name == "tsp") return std::make_unique<TspProblem>(6, 40, numVars, seed);
    if (name == "lis") return std::make_unique<LisProblem>(4, 30, numVars, seed);
    if (name == "substring") return std::make_unique<SubstringProblem>(5, 30, numVars, seed);
    if (name == "cycle") return std::make_unique<CycleDetectionProblem>(6, 30, numVars, seed);
    if (name == "dominated_window") return std::make_unique<DominatedWindowProblem>(5, 30, numVars, seed);
    if (name == "palindrome") return std::make_unique<PalindromeProblem>(numVars, seed);
    if (name == "inversion_count") return std::make_unique<InversionCountProblem>(numVars, seed);
    if (name == "has_duplicate") return std::make_unique<HasDuplicateProblem>(numVars, seed);
    if (name == "is_sorted") return std::make_unique<IsSortedProblem>(numVars, seed);
    if (name == "count_descents") return std::make_unique<CountDescentsProblem>(numVars, seed);
    if (name == "strictly_increasing") return std::make_unique<StrictlyIncreasingProblem>(numVars, seed);
    if (name == "has_adjacent_equal") return std::make_unique<HasAdjacentEqualProblem>(numVars, seed);
    if (name == "longest_run") return std::make_unique<LongestRunProblem>(numVars, seed);
    if (name == "max_adjacent_gap") return std::make_unique<MaxAdjacentGapProblem>(numVars, seed);
    if (name == "median") return std::make_unique<MedianProblem>(numVars, seed);
    if (name == "second_smallest") return std::make_unique<SecondSmallestProblem>(numVars, seed);
    if (name == "count_distinct") return std::make_unique<CountDistinctProblem>(numVars, seed);
    if (name == "min_gap") return std::make_unique<MinGapProblem>(numVars, seed);
    if (name == "max_gap_sorted") return std::make_unique<MaxGapSortedProblem>(numVars, seed);
    if (name == "max_subarray_sum") return std::make_unique<MaxSubarraySumProblem>(numVars, seed);
    if (name == "longest_increasing_run") return std::make_unique<LongestIncreasingRunProblem>(numVars, seed);
    if (name == "max_profit") return std::make_unique<MaxProfitProblem>(numVars, seed);
    if (name == "count_leaders") return std::make_unique<CountLeadersProblem>(numVars, seed);
    if (name == "has_majority") return std::make_unique<HasMajorityProblem>(numVars, seed);
    if (name == "trapping_rain_water") return std::make_unique<TrappingRainWaterProblem>(numVars, seed);
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
        config.postSolveSizes = args.postSolveSizes;
    config.tailEnumTop = args.tailEnumTop;
        config.tailEnumTop = args.tailEnumTop;
        config.genConfig.loopAccumulatorBias = args.loopBias;
        config.genConfig.indexIncrementBias = args.indexIncrementBias;
        config.genConfig.bareSwapPenalty = args.bareSwapPenalty;
        config.genConfig.controlFlowBias = args.controlFlowBias;
        config.genConfig.compositeBias = args.compositeBias;
        config.genConfig.exprMutationRate = args.exprMutationRate;
        config.genConfig.pruneGeneration = args.pruneGeneration;
        config.genConfig.roleTypedVars = args.roleTypedVars;
        config.genConfig.combinatorBias = args.combinatorBias;
        config.genConfig.combinatorAdjacent = args.combinatorAdjacent;
    config.genConfig.combinatorStep = args.combinatorStep;
        config.genConfig.combinatorStep = args.combinatorStep;
        config.genConfig.secondArrayBias = args.secondArrayBias;
        config.genConfig.recursionCallBias = args.recursionCallBias;

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
        res.generalizes =
            verifyGeneralization(*problemForEval, *c.program, kBenchmarkStepLimit, reason, args.recursionCallBias > 0.0);
        if (!res.generalizes) {
            res.failureReason = reason;
            rejected++;
            std::cout << "  REJECTED (does not generalize): " << reason << "\n";
            continue;
        }
        res.timings = timeProgram(*problemForEval, *c.program, kBenchmarkStepLimit, sizes, args.recursionCallBias > 0.0);
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


// Multiplies every edge weight of a TSP instance (the V*V matrix at
// input[1+4V..], see TspProblem.hpp) by k; 0 stays 0 (no edge). The expected
// optimum scales by k too (-1 = no tour stays -1).
Instance scaleTspWeights(const Instance& inst, int k) {
    Instance out = inst;
    int v = out.input[0];
    for (size_t i = static_cast<size_t>(1 + 4 * v); i < out.input.size(); ++i) out.input[i] *= k;
    if (!out.expectedOutput.empty() && out.expectedOutput[0] > 0) out.expectedOutput[0] *= k;
    return out;
}

// Idea A, FINDINGS.md section 41: fill the pruning guard of the TSP
// branch-and-bound shape by exhaustive enumeration (search/HoleFiller.hpp)
// instead of mutation. Training instances are V = 2..--max-n; the winner is
// then checked on V up to --max-n + 2 (generated and adversarial instances)
// it never saw. `--threads` evaluates candidate classes in parallel.
void runFillTspGuardMode(const CliArgs& args) {
    const std::vector<int> vars = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};  // nv size uBase chBase pcBase mBase best level c w cost
    const int numVars = 12;
    const long stepLimit = 50'000'000;
    TspProblem problem(6, 40, numVars, static_cast<uint64_t>(args.seed));
    std::vector<Instance> training;
    for (int v = 2; v <= args.maxN; ++v) {
        for (auto& inst : problem.generateInstances(v)) training.push_back(std::move(inst));
    }
    // --metamorphic (section 44): add copies of every training instance with
    // all edge weights multiplied by 10 and by 100. Scaling preserves every
    // tour's rank (and 0 = no edge), so a sound cost-based guard prunes the
    // same subtrees at every scale, while a guard comparing costs to layout
    // constants or literals changes behavior. Everything downstream --
    // snapshots, soundness labels, correctness, total steps -- then spans all
    // scales. k stays <= 100 so tour costs stay below the skeleton's 100000
    // "infinity" sentinel.
    std::vector<std::vector<Instance>> byScale;  // --rank-worst-scale: training split per scale
    if (args.metamorphic) {
        size_t base = training.size();
        for (int k : {10, 100}) {
            for (size_t i = 0; i < base; ++i) training.push_back(scaleTspWeights(training[i], k));
        }
        std::cout << "[fill] metamorphic: " << base << " instances x scales {1, 10, 100}\n";
        for (size_t sc = 0; sc < 3; ++sc) {
            byScale.emplace_back(training.begin() + static_cast<long>(sc * base), training.begin() + static_cast<long>((sc + 1) * base));
        }
    }

    // Snapshots of the scalars at every guard evaluation, with no pruning at
    // all (guard = 1), capped by uniform subsampling.
    // With --soundness-labels every evaluation is kept (arrays too) and
    // labeled by tspPruneIsSafe; otherwise a uniform 1-in-7 subsample.
    std::vector<std::vector<int>> snapshots;
    std::vector<char> safeToPrune;
    {
        TspBranchAndBound open = buildTspBranchAndBoundWithGuard(vars, makeIntConst(1));
        Interpreter interp(stepLimit);
        long visits = 0;
        interp.setConditionTrace(open.guardSite, [&](const std::vector<int>& v, const std::vector<int>& arr) {
            bool keep = args.soundnessLabels ? snapshots.size() < 200000 : (visits % 7 == 0 && snapshots.size() < 4000);
            visits++;
            if (!keep) return;
            snapshots.push_back(v);
            if (args.soundnessLabels) safeToPrune.push_back(tspPruneIsSafe(v, arr) ? 1 : 0);
        });
        std::vector<int> out;
        for (const auto& inst : training) interp.run(*open.program, inst.input, numVars, out);
        std::cout << "[fill] " << training.size() << " training instances (V=2.." << args.maxN << "), " << visits
                  << " guard evaluations, " << snapshots.size() << " snapshots kept\n";
        if (args.soundnessLabels) {
            long safe = std::count(safeToPrune.begin(), safeToPrune.end(), 1);
            std::cout << "[fill] soundness labels: " << safe << " evaluations where pruning is safe, "
                      << (static_cast<long>(safeToPrune.size()) - safe) << " where it would cut a strictly better tour\n";
        }
    }
    // Unsafe prunes of a guard: evaluations it would prune (value 0) although
    // a strictly better tour lies below.
    auto unsafePrunes = [&](const Expr& guard) {
        long n = 0;
        for (size_t k = 0; k < safeToPrune.size(); ++k) {
            if (!safeToPrune[k] && evalOnSnapshot(guard, snapshots[k]) == 0) n++;
        }
        return n;
    };

    std::vector<ExprPtr> all = enumerateConditions(vars, 3);
    size_t enumerated = all.size();
    std::vector<ExprPtr> reps = dedupeByTruthVector(std::move(all), snapshots);
    std::cout << "[fill] " << enumerated << " conditions enumerated, " << reps.size()
              << " observationally distinct on the snapshots\n";

    // Evaluate: correct on every training instance, total interpreter steps.
    auto evaluate = [&](const Expr& guard, const std::vector<Instance>& insts, long& steps) {
        TspBranchAndBound prog = buildTspBranchAndBoundWithGuard(vars, guard.clone());
        Interpreter interp(stepLimit);
        std::vector<int> out;
        steps = 0;
        for (const auto& inst : insts) {
            long s = 0;
            if (problem.evaluateInto(*prog.program, inst, interp, out, &s) != ExecStatus::SUCCESS) return false;
            steps += s;
        }
        return true;
    };
    std::vector<long> steps(reps.size(), -1);
    unsigned workers = args.threads > 0 ? static_cast<unsigned>(args.threads) : std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < workers; ++t) {
        pool.emplace_back([&, t] {
            for (size_t i = t; i < reps.size(); i += workers) {
                long st = 0;
                if (evaluate(*reps[i], training, st)) steps[i] = st;
            }
        });
    }
    for (auto& th : pool) th.join();

    long noPrune = 0, handGuard = 0;
    bool noPruneOk = evaluate(*makeIntConst(1), training, noPrune);
    bool handOk = evaluate(*makeCompare(ExprKind::Lt, makeVar(11), makeVar(7)), training, handGuard);  // cost < best
    if (!noPruneOk || !handOk) {
        // Sanity: both reference guards are sound, so a failure here means the
        // instance set (e.g. a metamorphic transform) is broken, not the guards.
        std::cout << "[fill] SANITY FAILURE: reference guard incorrect on the training set (no pruning "
                  << (noPruneOk ? "ok" : "FAILS") << ", cost < best " << (handOk ? "ok" : "FAILS") << ")\n";
    }
    std::vector<size_t> correct;
    long rejectedUnsound = 0;
    for (size_t i = 0; i < reps.size(); ++i) {
        if (steps[i] < 0) continue;
        if (args.soundnessLabels && unsafePrunes(*reps[i]) > 0) {
            rejectedUnsound++;  // correct on the training outputs, but prunes a better tour somewhere
            continue;
        }
        correct.push_back(i);
    }
    if (args.soundnessLabels) {
        std::cout << "[fill] " << rejectedUnsound << " classes correct on training outputs but rejected for unsafe prunes; "
                  << "hand-written `cost < best` has " << unsafePrunes(*makeCompare(ExprKind::Lt, makeVar(11), makeVar(7)))
                  << " unsafe prunes\n";
    }
    // --rank-worst-scale (section 44, attempt 2): rank by the WORST speedup
    // over no pruning across the weight scales instead of total steps -- a
    // guard that is fast at one scale and a no-op at the others then ranks
    // like no pruning, while a scale-invariant guard keeps its (smaller)
    // speedup everywhere. Summing, the first attempt's key, let one scale's
    // large gain outweigh the others.
    std::vector<double> worstSpeedup(reps.size(), 0.0);
    if (args.rankWorstScale && !byScale.empty()) {
        std::vector<long> noPruneScale(byScale.size(), 0);
        for (size_t sc = 0; sc < byScale.size(); ++sc) evaluate(*makeIntConst(1), byScale[sc], noPruneScale[sc]);
        std::vector<std::thread> rankers;
        for (unsigned t = 0; t < workers; ++t) {
            rankers.emplace_back([&, t] {
                for (size_t k = t; k < correct.size(); k += workers) {
                    size_t i = correct[k];
                    double worst = 1e18;
                    for (size_t sc = 0; sc < byScale.size(); ++sc) {
                        long st = 0;
                        evaluate(*reps[i], byScale[sc], st);
                        worst = std::min(worst, static_cast<double>(noPruneScale[sc]) / static_cast<double>(std::max(1L, st)));
                    }
                    worstSpeedup[i] = worst;
                }
            });
        }
        for (auto& th : rankers) th.join();
        std::sort(correct.begin(), correct.end(), [&](size_t a, size_t b) {
            if (worstSpeedup[a] != worstSpeedup[b]) return worstSpeedup[a] > worstSpeedup[b];
            return steps[a] < steps[b];
        });
        std::cout << "[fill] ranking by worst per-scale speedup; top: ";
        for (size_t k = 0; k < correct.size() && k < 5; ++k) {
            std::cout << reps[correct[k]]->toString() << " (worst x" << std::fixed << std::setprecision(3)
                      << worstSpeedup[correct[k]] << ")  ";
        }
        std::cout << "\n";
    } else {
        std::sort(correct.begin(), correct.end(), [&](size_t a, size_t b) { return steps[a] < steps[b]; });
    }
    std::cout << "[fill] " << correct.size() << " classes correct on every training instance"
              << (args.soundnessLabels ? " with zero unsafe prunes" : "") << "\n"
              << "[fill] baselines: no pruning " << noPrune << " steps, hand-written `cost < best` " << handGuard << " steps\n"
              << "[fill] fastest correct guards (v7 = best, v11 = cost, v10 = w, v8 = level):\n";
    for (size_t k = 0; k < correct.size() && k < 15; ++k) {
        size_t i = correct[k];
        std::cout << "  " << std::setw(12) << steps[i] << " steps  x" << std::fixed << std::setprecision(2)
                  << static_cast<double>(noPrune) / static_cast<double>(steps[i]) << " vs no pruning  " << reps[i]->toString() << "\n";
    }
    {
        // Where the hand-written guard's class lands among the correct ones.
        size_t rank = 0;
        if (args.rankWorstScale && !byScale.empty()) {
            ExprPtr hand = makeCompare(ExprKind::Lt, makeVar(11), makeVar(7));
            double handWorst = 1e18;
            for (const auto& set : byScale) {
                long np = 0, st = 0;
                evaluate(*makeIntConst(1), set, np);
                evaluate(*hand, set, st);
                handWorst = std::min(handWorst, static_cast<double>(np) / static_cast<double>(std::max(1L, st)));
            }
            for (size_t i : correct) rank += worstSpeedup[i] > handWorst ? 1 : 0;
            std::cout << "[fill] `cost < best` worst per-scale speedup x" << std::fixed << std::setprecision(3) << handWorst << "\n";
        } else {
            for (size_t i : correct) rank += steps[i] < handGuard ? 1 : 0;
        }
        std::cout << "[fill] hand-written `cost < best` would rank #" << (rank + 1) << " of " << correct.size() << " by steps\n";
    }
    if (correct.empty()) return;

    // Held-out check of the winner on larger V than it was selected on.
    const Expr& winner = *reps[correct.front()];
    for (int v = args.maxN + 1; v <= args.maxN + 2; ++v) {
        std::vector<Instance> heldOut = problem.generateInstances(v);
        for (auto& inst : problem.generateAdversarialInstances(v)) heldOut.push_back(std::move(inst));
        if (args.metamorphic) {  // held-out at the unseen scale 7 as well
            size_t n = heldOut.size();
            for (size_t i = 0; i < n; ++i) heldOut.push_back(scaleTspWeights(heldOut[i], 7));
        }
        long wSteps = 0, hSteps = 0, nSteps = 0;
        bool ok = evaluate(winner, heldOut, wSteps);
        evaluate(*makeCompare(ExprKind::Lt, makeVar(11), makeVar(7)), heldOut, hSteps);
        evaluate(*makeIntConst(1), heldOut, nSteps);
        std::cout << "[fill] held-out V=" << v << " (" << heldOut.size() << " instances): winner "
                  << (ok ? "correct" : "WRONG") << ", steps winner " << wSteps << " / hand " << hSteps << " / no pruning " << nSteps
                  << "\n";
    }
}

int main(int argc, char** argv) {
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage();
        return 1;
    }
    resolveNumVars(args);

    std::unique_ptr<Problem> problem = makeProblem(args.problem, args.numVars, args.seed);
    if (!problem) {
        std::cerr << "Unknown problem: " << args.problem
                   << " (available: sorting, min, max, sum, contains, contains_ordered, is_prime, gcd, mod, factorial, fibonacci, "
                      "count, average, reverse, power, three_sum, single_number, graph_reachability, shortest_path, tsp, dtw, "
                      "lis, substring, cycle, dominated_window)\n";
        return 1;
    }

    if (args.fillTspGuard) {
        runFillTspGuardMode(args);
        return 0;
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
    config.postSolveSizes = args.postSolveSizes;
    config.tailEnumTop = args.tailEnumTop;
    config.genConfig.loopAccumulatorBias = args.loopBias;
    config.genConfig.indexIncrementBias = args.indexIncrementBias;
    config.genConfig.bareSwapPenalty = args.bareSwapPenalty;
    config.genConfig.controlFlowBias = args.controlFlowBias;
    config.genConfig.compositeBias = args.compositeBias;
    config.genConfig.exprMutationRate = args.exprMutationRate;
    config.genConfig.pruneGeneration = args.pruneGeneration;
    config.genConfig.roleTypedVars = args.roleTypedVars;
    config.genConfig.combinatorBias = args.combinatorBias;
    config.genConfig.combinatorAdjacent = args.combinatorAdjacent;
    config.genConfig.combinatorStep = args.combinatorStep;
    config.genConfig.adaptiveSkeletonBiasRate = args.adaptiveSkeletonBiasRate;
    config.genConfig.secondArrayBias = args.secondArrayBias;
    config.genConfig.recursionCallBias = args.recursionCallBias;
    config.adaptiveBiasLogPath = args.adaptiveBiasLog;
    config.checkpointPath = args.checkpointPath;

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
