#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>

#include "problems/AverageProblem.hpp"
#include "problems/ContainsAscendingProblem.hpp"
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
#include "problems/LongestNonIncreasingRunProblem.hpp"
#include "problems/MinSubarraySumProblem.hpp"
#include "problems/LongestDecreasingSubsequenceProblem.hpp"
#include "problems/MaxSumIncreasingSubsequenceProblem.hpp"
#include "problems/CeilingWaterProblem.hpp"
#include "problems/MaxRiseProblem.hpp"
#include "problems/MinStepProblem.hpp"
#include "problems/CountRunsProblem.hpp"
#include "problems/CountAscendingRunsProblem.hpp"
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
#include "problems/FrequentValuesProblem.hpp"
#include "problems/HasDoubleValueProblem.hpp"
#include "problems/OddFrequencyValuesProblem.hpp"
#include "problems/RepeatedValuesProblem.hpp"
#include "problems/RareValuesProblem.hpp"
#include "problems/SumDistinctProblem.hpp"
#include "problems/EqualPairsProblem.hpp"
#include "problems/ArgmaxIndexProblem.hpp"
#include "problems/LeastFrequentValueProblem.hpp"
#include "problems/MajorityValueProblem.hpp"
#include "problems/LongestRunValueProblem.hpp"
#include "problems/FirstMinusLastProblem.hpp"
#include "problems/LargestDropStepProblem.hpp"
#include "problems/SumOfRisesProblem.hpp"
#include "problems/FirstGreaterThanFirstProblem.hpp"
#include "problems/SecondIndexOfKeyProblem.hpp"
#include "problems/CountBalancePointsProblem.hpp"
#include "problems/FirstSplitTwoToOneProblem.hpp"
#include "problems/AnyDuplicateProblem.hpp"
#include "problems/AnyPeakProblem.hpp"
#include "problems/AnyValleyProblem.hpp"
#include "problems/PeakElementProblem.hpp"
#include "problems/RotatedMinProblem.hpp"
#include "problems/SearchRotatedProblem.hpp"
#include "problems/EquilibriumPointProblem.hpp"
#include "problems/LongestIncreasingRunEndProblem.hpp"
#include "problems/FrequentModeOrNoneProblem.hpp"
#include "problems/LongestRunStartProblem.hpp"
#include "problems/PluralityValueProblem.hpp"
#include "problems/SumUniqueValuesProblem.hpp"
#include "problems/EvenFrequencyValuesProblem.hpp"
#include "problems/MaxRepeatedValueProblem.hpp"
#include "problems/MinRepeatedValueProblem.hpp"
#include "problems/TripleFrequencyValuesProblem.hpp"
#include "problems/SumExcessOverFirstProblem.hpp"
#include "problems/SumOfDropsProblem.hpp"
#include "problems/SumSquaredStepsProblem.hpp"
#include "problems/SumAbsFromLastProblem.hpp"
#include "problems/SumAbsFromFirstProblem.hpp"
#include "problems/ListQueryProblem.hpp"
#include "problems/ElementsTwiceProblem.hpp"
#include "problems/MinFrequencyProblem.hpp"
#include "problems/ModeProblem.hpp"
#include "problems/ValuesTwiceProblem.hpp"
#include "problems/MaxFrequencyProblem.hpp"
#include "problems/CountUniqueProblem.hpp"
#include "problems/CountDominatedProblem.hpp"
#include "problems/MaxDrawdownProblem.hpp"
#include "problems/TotalVariationProblem.hpp"
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
#include "search/SkeletonAblation.hpp"

using namespace algosearch;

namespace {

struct CliArgs {
    std::string problem = "sorting";
    int maxN = 5;
    int beamWidth = 500;
    int maxProgramSize = 30;
    long stepLimit = 5000;
    uint64_t seed = 1234;
    bool seedGiven = false;
    int restartOnStall = 0;  // section 94
    double wallSeconds = 0.0;
    double profileNovelty = 0.0;  // section 98
    bool profileBan = false;      // section 118
    unsigned forbidStmtMask = 0;  // section 118b
    int confirmReseed = 12;  // default since section 97; --confirm-reseed 0 turns it off
    int startLevel = 1;
    int candidatesPerSize = 800;
    int numVars = 4;
    bool numVarsExplicit = false;  // true once --num-vars is parsed; see resolveNumVars
    unsigned threads = 0;  // 0 = auto-detect
    int invarianceLookahead = 3;
    double diversityRate = 0.0;
    double crossoverRate = 0.0;
    double shapeNicheRate = 0.0;
    double behaviorNicheRate = 0.0;
    double speedBonusWeight = 0.0;
    int postSolveSizes = 0;
    int tailEnumTop = 10;  // default since FINDINGS.md section 54; --tail-enum 0 restores the old default
    double loopBias = 0.4;  // best value found by a 160-run sweep, see ProgramGenerator.hpp
    double compositeBias = 0.0;
    double localCondMutationRate = 0.3;  // FINDINGS.md section 115b, default
    bool pruneGeneration = false;
    bool roleTypedVars = false;
    double combinatorBias = 0.5;  // default since FINDINGS.md section 48 (A/B in section 47); --combinator-bias 0 restores the old default
    bool combinatorAdjacent = true;  // v2 axes by default; --combinator-v1 restores the frozen v1 family
    bool combinatorState = true;  // default since FINDINGS.md section 62; --no-combinator-state restores the old default
    bool combinatorNested = true;  // default since FINDINGS.md section 72; --no-combinator-nested restores the old default
    bool nestedCanonicalTests = false;  // FINDINGS.md section 73
    bool combinatorFrequency = true;  // default since FINDINGS.md section 76; --no-combinator-frequency restores the old default
    // Gated follow blocks are the default since FINDINGS.md section 79 (--no-follow-pointer-inputs turns them off).
    bool combinatorFollow = true;          // FINDINGS.md section 77
    bool followPointerInputsOnly = true;   // FINDINGS.md section 79
    bool combinatorOrthogonal = true;  // default since FINDINGS.md section 81; --no-combinator-orthogonal restores the old default
    bool combinatorArgBest = true;  // default since FINDINGS.md section 85; --no-combinator-argbest restores the old default
    bool combinatorDistinct = true;  // default since FINDINGS.md section 88; --no-combinator-distinct restores the old default
    bool followExtended = true;  // default since FINDINGS.md section 90; --no-follow-extended restores the old default
    bool scalarLoop = true;  // default since FINDINGS.md section 100; --no-scalar-loop restores the old default
    bool nestedPlace = false;  // FINDINGS.md section 102, opt-in
    bool nestedPlaceOption = false;  // FINDINGS.md section 102b, opt-in
    bool combinatorTriple = true;  // FINDINGS.md section 103, default
    bool combinatorRelax = true;  // FINDINGS.md section 104, default
    bool combinatorGrid = true;  // FINDINGS.md section 109, default
    bool combinatorTransform = true;  // FINDINGS.md section 111, default
    bool distinctAggregate = true;  // FINDINGS.md section 112d, default
    bool argSentinel = true;  // FINDINGS.md section 113, default
    bool firstIndex = false;  // FINDINGS.md section 117, opt-in
    bool prefixSplit = false;  // FINDINGS.md section 117, opt-in
    bool scalarLoopIndexRange = true;  // FINDINGS.md section 101, default
    int regressionGuard = 12;  // default since FINDINGS.md section 89; --regression-guard 0 restores the old default
    bool combinatorStep = true;  // default since FINDINGS.md section 53; --no-combinator-step restores the old default
    double adaptiveSkeletonBiasRate = 0.0;
    double secondArrayBias = 0.0;
    double recursionCallBias = 0.0;
    double dcShellBias = 0.0;  // section 119
    double dcShellSearchBias = 0.0;  // section 121
    double dcShellSearchKey = 0.0;  // section 121c
    double dcShellSearchEnd = 0.0;  // section 123
    std::string adaptiveBiasLog;
    std::string output = "solutions.json";
    std::string checkpointPath;
    bool benchmark = false;
    bool fillTspGuard = false;
    bool fillBsearchHole = false;  // FINDINGS.md section 63
    int bsearchStage = 3;
    std::string ablateSkeleton;  // FINDINGS.md section 65
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
        else if (arg == "--seed") { args.seed = static_cast<uint64_t>(std::stoull(next("--seed"))); args.seedGiven = true; }
        else if (arg == "--restart-on-stall") args.restartOnStall = std::stoi(next("--restart-on-stall"));
        else if (arg == "--wall-seconds") args.wallSeconds = std::stod(next("--wall-seconds"));
        else if (arg == "--confirm-reseed") args.confirmReseed = std::stoi(next("--confirm-reseed"));
        else if (arg == "--forbid-stmt") {
            std::string k = next("--forbid-stmt");
            if (k == "swap") args.forbidStmtMask |= 1u << static_cast<unsigned>(StmtKind::Swap);
            else if (k == "call") args.forbidStmtMask |= 1u << static_cast<unsigned>(StmtKind::Call);
            else if (k == "assignarray") args.forbidStmtMask |= 1u << static_cast<unsigned>(StmtKind::AssignArray);
            else { std::cerr << "--forbid-stmt takes swap|call|assignarray\n"; std::exit(1); }
        }
        else if (arg == "--profile-ban") args.profileBan = true;
        else if (arg == "--profile-novelty") args.profileNovelty = std::stod(next("--profile-novelty"));
        else if (arg == "--start-level") args.startLevel = std::stoi(next("--start-level"));
        else if (arg == "--candidates") args.candidatesPerSize = std::stoi(next("--candidates"));
        else if (arg == "--num-vars") { args.numVars = std::stoi(next("--num-vars")); args.numVarsExplicit = true; }
        else if (arg == "--threads") args.threads = static_cast<unsigned>(std::stoul(next("--threads")));
        else if (arg == "--invariance-lookahead") args.invarianceLookahead = std::stoi(next("--invariance-lookahead"));
        else if (arg == "--diversity-rate") args.diversityRate = std::stod(next("--diversity-rate"));
        else if (arg == "--crossover-rate") args.crossoverRate = std::stod(next("--crossover-rate"));
        else if (arg == "--shape-niche-rate") args.shapeNicheRate = std::stod(next("--shape-niche-rate"));
        else if (arg == "--behavior-niche-rate") args.behaviorNicheRate = std::stod(next("--behavior-niche-rate"));
        else if (arg == "--speed-bonus-weight") args.speedBonusWeight = std::stod(next("--speed-bonus-weight"));
        else if (arg == "--post-solve-sizes") args.postSolveSizes = std::stoi(next("--post-solve-sizes"));
        else if (arg == "--tail-enum") args.tailEnumTop = std::stoi(next("--tail-enum"));
        else if (arg == "--loop-bias") args.loopBias = std::stod(next("--loop-bias"));
        else if (arg == "--composite-bias") args.compositeBias = std::stod(next("--composite-bias"));
        else if (arg == "--local-cond-mutation") args.localCondMutationRate = std::stod(next("--local-cond-mutation"));
        else if (arg == "--no-local-cond-mutation") args.localCondMutationRate = 0.0;
        else if (arg == "--prune-generation") args.pruneGeneration = true;
        else if (arg == "--role-typed-vars") args.roleTypedVars = true;
        else if (arg == "--combinator-bias") args.combinatorBias = std::stod(next("--combinator-bias"));
        else if (arg == "--combinator-adjacent") args.combinatorAdjacent = true;  // now the default; kept so section 40's scripts still run
        else if (arg == "--combinator-v1") args.combinatorAdjacent = false;
        else if (arg == "--no-combinator-step") args.combinatorStep = false;
        else if (arg == "--no-combinator-state") args.combinatorState = false;
        else if (arg == "--no-combinator-nested") args.combinatorNested = false;
        else if (arg == "--nested-canonical-tests") args.nestedCanonicalTests = true;
        else if (arg == "--no-combinator-frequency") args.combinatorFrequency = false;
        else if (arg == "--combinator-follow") args.combinatorFollow = true, args.followPointerInputsOnly = false;  // ungated (section 77)
        else if (arg == "--no-follow-pointer-inputs") args.combinatorFollow = args.followPointerInputsOnly = false;
        else if (arg == "--no-combinator-orthogonal") args.combinatorOrthogonal = false;
        else if (arg == "--no-combinator-argbest") args.combinatorArgBest = false;
        else if (arg == "--no-combinator-distinct") args.combinatorDistinct = false;
        else if (arg == "--regression-guard") args.regressionGuard = std::stoi(next("--regression-guard"));
        else if (arg == "--no-follow-extended") args.followExtended = false;
        else if (arg == "--no-scalar-loop") args.scalarLoop = false;
        else if (arg == "--no-scalar-loop-index-range") args.scalarLoopIndexRange = false;
        else if (arg == "--nested-place") args.nestedPlace = true;
        else if (arg == "--nested-place-option") args.nestedPlaceOption = true;
        else if (arg == "--no-combinator-triple") args.combinatorTriple = false;
        else if (arg == "--no-combinator-relax") args.combinatorRelax = false;
        else if (arg == "--no-combinator-grid") args.combinatorGrid = false;
        else if (arg == "--no-combinator-transform") args.combinatorTransform = false;
        else if (arg == "--no-distinct-aggregate") args.distinctAggregate = false;
        else if (arg == "--no-arg-sentinel") args.argSentinel = false;
        else if (arg == "--first-index") args.firstIndex = true;
        else if (arg == "--no-first-index") args.firstIndex = false;
        else if (arg == "--prefix-split") args.prefixSplit = true;
        else if (arg == "--no-prefix-split") args.prefixSplit = false;
        else if (arg == "--adaptive-skeleton-bias-rate") args.adaptiveSkeletonBiasRate = std::stod(next("--adaptive-skeleton-bias-rate"));
        else if (arg == "--second-array-bias") args.secondArrayBias = std::stod(next("--second-array-bias"));
        else if (arg == "--dc-shell") args.dcShellBias = std::stod(next("--dc-shell"));
        else if (arg == "--dc-shell-search") args.dcShellSearchBias = std::stod(next("--dc-shell-search"));
        else if (arg == "--dc-shell-search-end") args.dcShellSearchEnd = std::stod(next("--dc-shell-search-end"));
        else if (arg == "--dc-shell-search-key") args.dcShellSearchKey = std::stod(next("--dc-shell-search-key"));
        else if (arg == "--recursion-call-bias") args.recursionCallBias = std::stod(next("--recursion-call-bias"));
        else if (arg == "--adaptive-bias-log") args.adaptiveBiasLog = next("--adaptive-bias-log");
        else if (arg == "--output") args.output = next("--output");
        else if (arg == "--checkpoint-path") args.checkpointPath = next("--checkpoint-path");
        else if (arg == "--benchmark") args.benchmark = true;
        else if (arg == "--fill-tsp-guard") args.fillTspGuard = true;
        else if (arg == "--fill-bsearch-hole") args.fillBsearchHole = true;
        else if (arg == "--bsearch-stage") args.bsearchStage = std::stoi(next("--bsearch-stage"));
        else if (arg == "--ablate-skeleton") args.ablateSkeleton = next("--ablate-skeleton");
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
    std::cout << "algorithm-discovery --problem sorting|min|max|sum|contains|contains_ordered|is_prime|gcd|factorial|fibonacci|count|average|reverse|power|three_sum|single_number|graph_reachability|shortest_path|tsp|dtw|lis|substring|cycle|dominated_window|palindrome|inversion_count|has_duplicate|is_sorted|count_descents|strictly_increasing|has_adjacent_equal|longest_run|max_adjacent_gap|median|second_smallest|count_distinct|min_gap|max_gap_sorted|max_subarray_sum|longest_increasing_run|max_profit|count_leaders|has_majority|trapping_rain_water|max_rise|min_step|count_runs|count_ascending_runs|longest_non_increasing_run|min_subarray_sum|longest_decreasing_subsequence|max_sum_increasing_subsequence|ceiling_water|max_frequency|count_unique|count_dominated|max_drawdown|total_variation|elements_twice|min_frequency|mode|values_twice|walk_length|visits_zero|cycle_length|last_node|first_minus_last|largest_drop_step|sum_of_rises|sum_abs_from_first|argmax_index|least_frequent_value|majority_value|longest_run_value|repeated_values|rare_values|sum_distinct|equal_pairs|frequent_values|has_double_value|odd_frequency_values|cycle_entry|second_last_node|sum_excess_over_first|sum_of_drops|sum_squared_steps|sum_abs_from_last|sum_unique_values|even_frequency_values|max_repeated_value|min_repeated_value|longest_increasing_run_end|frequent_mode_or_none|longest_run_start|plurality_value|peak_element|search_rotated|equilibrium_point|first_greater_than_first|second_index_of_key|count_balance_points|first_split_two_to_one|triple_frequency_values|any_peak|any_duplicate|any_valley|contains_ascending|rotated_min --max-n 5 --beam-width 1000 "
                 "--max-program-size 30 --step-limit 10000 --seed 1234 "
                 "[--start-level 1] [--candidates 800] [--num-vars 4 (auto-raised per-problem if omitted; see resolveNumVars)] [--threads 0] "
                 "[--invariance-lookahead 3] [--diversity-rate 0.0] [--crossover-rate 0.0] [--shape-niche-rate 0.0] "
                 "[--behavior-niche-rate 0.0] [--speed-bonus-weight 0.0] [--post-solve-sizes 0] [--tail-enum 10] [--loop-bias 0.4] "
                 "[--composite-bias 0.0] [--local-cond-mutation 0.3] [--no-local-cond-mutation] [--prune-generation] [--role-typed-vars] [--combinator-bias 0.5] [--combinator-v1] [--no-combinator-step] [--no-combinator-state] [--no-combinator-nested] [--nested-canonical-tests] [--no-combinator-frequency] [--combinator-follow] [--no-follow-pointer-inputs] [--no-combinator-orthogonal] [--no-combinator-argbest] [--no-combinator-distinct] [--regression-guard 12] [--no-follow-extended] [--no-scalar-loop] [--no-scalar-loop-index-range] [--nested-place] [--nested-place-option] [--no-combinator-triple] [--no-combinator-relax] [--no-combinator-grid] [--no-combinator-transform] [--no-distinct-aggregate] [--no-arg-sentinel] [--first-index] [--prefix-split] "
                 "[--adaptive-skeleton-bias-rate 0.0] [--adaptive-bias-log path.csv] "
                 "[--second-array-bias 0.0] [--recursion-call-bias 0.0] [--dc-shell 0.0] [--dc-shell-search 0.0] [--dc-shell-search-key 0.0] [--dc-shell-search-end 0.0] "
                 "[--output solutions.json] [--checkpoint-path path.json] [--restart-on-stall K] [--wall-seconds S] [--confirm-reseed L] [--profile-novelty W] [--profile-ban] [--forbid-stmt swap|call|assignarray] "
                 "[--benchmark] [--benchmark-seeds 5] [--fill-tsp-guard [--soundness-labels] [--metamorphic [--rank-worst-scale]]] [--fill-bsearch-hole [--bsearch-stage 3]] [--ablate-skeleton NAME]\n";
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
    if (problem == "tsp" || problem == "dtw") return 12;           // Tsp skeletons; dtw: grid combinator piece
    if (problem == "shortest_path") return 7;                       // relax piece, INF init
    if (problem == "graph_reachability") return 6;                  // relax combinator piece
    if (problem == "three_sum") return 5;                           // triple combinator piece
    if (problem == "average") return 5;                             // scalar-loop index-range fold
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
    if (name == "rotated_min") return std::make_unique<RotatedMinProblem>(numVars, seed);
    if (name == "contains_ascending") return std::make_unique<ContainsAscendingProblem>(6, 40, numVars, seed);
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
    if (name == "walk_length") return std::make_unique<WalkLengthProblem>(numVars, seed);
    if (name == "visits_zero") return std::make_unique<VisitsZeroProblem>(numVars, seed);
    if (name == "cycle_length") return std::make_unique<CycleLengthProblem>(numVars, seed);
    if (name == "last_node") return std::make_unique<LastNodeProblem>(numVars, seed);
    if (name == "cycle_entry") return std::make_unique<CycleEntryProblem>(numVars, seed);
    if (name == "second_last_node") return std::make_unique<SecondLastNodeProblem>(numVars, seed);
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
    if (name == "longest_non_increasing_run") return std::make_unique<LongestNonIncreasingRunProblem>(numVars, seed);
    if (name == "min_subarray_sum") return std::make_unique<MinSubarraySumProblem>(numVars, seed);
    if (name == "longest_decreasing_subsequence") return std::make_unique<LongestDecreasingSubsequenceProblem>(numVars, seed);
    if (name == "max_sum_increasing_subsequence") return std::make_unique<MaxSumIncreasingSubsequenceProblem>(numVars, seed);
    if (name == "ceiling_water") return std::make_unique<CeilingWaterProblem>(numVars, seed);
    if (name == "max_rise") return std::make_unique<MaxRiseProblem>(numVars, seed);
    if (name == "min_step") return std::make_unique<MinStepProblem>(numVars, seed);
    if (name == "count_runs") return std::make_unique<CountRunsProblem>(numVars, seed);
    if (name == "count_ascending_runs") return std::make_unique<CountAscendingRunsProblem>(numVars, seed);
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
    if (name == "frequent_values") return std::make_unique<FrequentValuesProblem>(numVars, seed);
    if (name == "has_double_value") return std::make_unique<HasDoubleValueProblem>(numVars, seed);
    if (name == "odd_frequency_values") return std::make_unique<OddFrequencyValuesProblem>(numVars, seed);
    if (name == "repeated_values") return std::make_unique<RepeatedValuesProblem>(numVars, seed);
    if (name == "rare_values") return std::make_unique<RareValuesProblem>(numVars, seed);
    if (name == "sum_distinct") return std::make_unique<SumDistinctProblem>(numVars, seed);
    if (name == "equal_pairs") return std::make_unique<EqualPairsProblem>(numVars, seed);
    if (name == "argmax_index") return std::make_unique<ArgmaxIndexProblem>(numVars, seed);
    if (name == "least_frequent_value") return std::make_unique<LeastFrequentValueProblem>(numVars, seed);
    if (name == "majority_value") return std::make_unique<MajorityValueProblem>(numVars, seed);
    if (name == "longest_run_value") return std::make_unique<LongestRunValueProblem>(numVars, seed);
    if (name == "first_minus_last") return std::make_unique<FirstMinusLastProblem>(numVars, seed);
    if (name == "largest_drop_step") return std::make_unique<LargestDropStepProblem>(numVars, seed);
    if (name == "sum_of_rises") return std::make_unique<SumOfRisesProblem>(numVars, seed);
    if (name == "first_greater_than_first") return std::make_unique<FirstGreaterThanFirstProblem>(numVars, seed);
    if (name == "second_index_of_key") return std::make_unique<SecondIndexOfKeyProblem>(numVars, seed);
    if (name == "count_balance_points") return std::make_unique<CountBalancePointsProblem>(numVars, seed);
    if (name == "first_split_two_to_one") return std::make_unique<FirstSplitTwoToOneProblem>(numVars, seed);
    if (name == "any_peak") return std::make_unique<AnyPeakProblem>(numVars, seed);
    if (name == "any_valley") return std::make_unique<AnyValleyProblem>(numVars, seed);
    if (name == "any_duplicate") return std::make_unique<AnyDuplicateProblem>(numVars, seed);
    if (name == "peak_element") return std::make_unique<PeakElementProblem>(numVars, seed);
    if (name == "search_rotated") return std::make_unique<SearchRotatedProblem>(numVars, seed);
    if (name == "equilibrium_point") return std::make_unique<EquilibriumPointProblem>(numVars, seed);
    if (name == "longest_increasing_run_end") return std::make_unique<LongestIncreasingRunEndProblem>(numVars, seed);
    if (name == "frequent_mode_or_none") return std::make_unique<FrequentModeOrNoneProblem>(numVars, seed);
    if (name == "longest_run_start") return std::make_unique<LongestRunStartProblem>(numVars, seed);
    if (name == "plurality_value") return std::make_unique<PluralityValueProblem>(numVars, seed);
    if (name == "sum_unique_values") return std::make_unique<SumUniqueValuesProblem>(numVars, seed);
    if (name == "even_frequency_values") return std::make_unique<EvenFrequencyValuesProblem>(numVars, seed);
    if (name == "min_repeated_value") return std::make_unique<MinRepeatedValueProblem>(numVars, seed);
    if (name == "max_repeated_value") return std::make_unique<MaxRepeatedValueProblem>(numVars, seed);
    if (name == "triple_frequency_values") return std::make_unique<TripleFrequencyValuesProblem>(numVars, seed);
    if (name == "sum_excess_over_first") return std::make_unique<SumExcessOverFirstProblem>(numVars, seed);
    if (name == "sum_of_drops") return std::make_unique<SumOfDropsProblem>(numVars, seed);
    if (name == "sum_squared_steps") return std::make_unique<SumSquaredStepsProblem>(numVars, seed);
    if (name == "sum_abs_from_last") return std::make_unique<SumAbsFromLastProblem>(numVars, seed);
    if (name == "sum_abs_from_first") return std::make_unique<SumAbsFromFirstProblem>(numVars, seed);
    if (name == "elements_twice") return std::make_unique<ElementsTwiceProblem>(numVars, seed);
    if (name == "min_frequency") return std::make_unique<MinFrequencyProblem>(numVars, seed);
    if (name == "mode") return std::make_unique<ModeProblem>(numVars, seed);
    if (name == "values_twice") return std::make_unique<ValuesTwiceProblem>(numVars, seed);
    if (name == "max_frequency") return std::make_unique<MaxFrequencyProblem>(numVars, seed);
    if (name == "count_unique") return std::make_unique<CountUniqueProblem>(numVars, seed);
    if (name == "count_dominated") return std::make_unique<CountDominatedProblem>(numVars, seed);
    if (name == "max_drawdown") return std::make_unique<MaxDrawdownProblem>(numVars, seed);
    if (name == "total_variation") return std::make_unique<TotalVariationProblem>(numVars, seed);
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
        config.behaviorNicheRate = args.behaviorNicheRate;
        config.speedBonusWeight = args.speedBonusWeight;
        config.postSolveSizes = args.postSolveSizes;
        config.tailEnumTop = args.tailEnumTop;
        config.regressionGuard = args.regressionGuard;
        config.genConfig.loopAccumulatorBias = args.loopBias;
        config.genConfig.compositeBias = args.compositeBias;
    config.genConfig.localCondMutationRate = args.localCondMutationRate;
        config.genConfig.localCondMutationRate = args.localCondMutationRate;
        config.genConfig.pruneGeneration = args.pruneGeneration;
        config.genConfig.roleTypedVars = args.roleTypedVars;
        config.genConfig.combinatorBias = args.combinatorBias;
        config.genConfig.combinatorAdjacent = args.combinatorAdjacent;
        config.genConfig.combinatorStep = args.combinatorStep;
        config.genConfig.combinatorState = args.combinatorState;
        config.genConfig.combinatorNested = args.combinatorNested;
        config.genConfig.nestedCanonicalTests = args.nestedCanonicalTests;
        config.genConfig.combinatorFrequency = args.combinatorFrequency;
        config.genConfig.combinatorFollow = args.combinatorFollow;
        config.genConfig.followPointerInputsOnly = args.followPointerInputsOnly;
        config.genConfig.followExtended = args.followExtended;
        config.genConfig.scalarLoop = args.scalarLoop;
        config.genConfig.scalarLoopIndexRange = args.scalarLoopIndexRange;
        config.genConfig.nestedPlace = args.nestedPlace;
        config.genConfig.nestedPlaceOption = args.nestedPlaceOption;
        config.genConfig.combinatorTriple = args.combinatorTriple;
        config.genConfig.combinatorRelax = args.combinatorRelax;
        config.genConfig.combinatorGrid = args.combinatorGrid;
    config.genConfig.combinatorTransform = args.combinatorTransform;
    config.genConfig.distinctAggregate = args.distinctAggregate;
    config.genConfig.argSentinel = args.argSentinel;
    config.genConfig.firstIndex = args.firstIndex;
    config.genConfig.prefixSplit = args.prefixSplit;
        config.genConfig.combinatorTransform = args.combinatorTransform;
        config.genConfig.distinctAggregate = args.distinctAggregate;
        config.genConfig.argSentinel = args.argSentinel;
        config.genConfig.firstIndex = args.firstIndex;
        config.genConfig.prefixSplit = args.prefixSplit;
        config.genConfig.combinatorOrthogonal = args.combinatorOrthogonal;
        config.genConfig.combinatorArgBest = args.combinatorArgBest;
        config.genConfig.combinatorDistinct = args.combinatorDistinct;
        config.genConfig.secondArrayBias = args.secondArrayBias;
        config.genConfig.recursionCallBias = args.recursionCallBias;
        config.genConfig.dcShellBias = args.dcShellBias;
        config.genConfig.dcShellSearchBias = args.dcShellSearchBias;
    config.genConfig.dcShellSearchKey = args.dcShellSearchKey;
    config.genConfig.dcShellSearchEnd = args.dcShellSearchEnd;

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

// Section 63: section 22's binary-search ladder with holes filled by
// exhaustive enumeration instead of mutation. Stage 3 (default): the hole is
// the not-found decision `if (C2) { A } else { B }`. Stage 2
// (`--bsearch-stage 2`): the found-check is a hole too,
// `if (C1) { F } else { if (C2) { A } else { B } }`, with F one of two
// loop-exiting bodies. C1 candidates are pruned by the instances' own labels:
// a C1 true at a state traced on an ABSENT instance would set found = 1 and
// answer wrongly, so it is dropped; a C1 never true on any present-instance
// state can never answer 1. Training is contains_ordered at n = 2..--max-n;
// correct candidates are ranked by total training steps; winners tied on
// steps are checked on held-out n = --max-n+1..--max-n+8, and every survivor
// by its step growth from n = 16 to 128 (O(log n) ~1.5x, O(n) ~7x).
void runFillBsearchHoleMode(const CliArgs& args) {
    const std::vector<int> skel = {1, 2, 3, 4, 5, 6, 7};  // target lo hi mid v dir found
    const int numVars = 9;
    const long stepLimit = 1'000'000;
    const bool stage2 = args.bsearchStage == 2;
    ContainsOrderedProblem problem(6, 40, numVars, static_cast<uint64_t>(args.seed));
    std::vector<Instance> training;
    for (int n = 2; n <= args.maxN; ++n) {
        for (auto& inst : problem.generateInstances(n)) training.push_back(std::move(inst));
    }
    static const char* const moveNames[8] = {"lo = mid + 1", "lo = mid", "lo = mid - 1", "hi = mid",
                                             "hi = mid - 1", "hi = mid + 1", "lo = lo + 1", "hi = hi - 1"};
    static const char* const foundNames[2] = {"found = 1; hi = lo", "found = 1; lo = hi"};
    auto move = [](int k) -> StmtPtr {
        const int lo = 2, hi = 3, mid = 4;
        switch (k) {
            case 0: return makeAssign(lo, makeBinOp(ExprKind::Add, makeVar(mid), makeIntConst(1)));
            case 1: return makeAssign(lo, makeVar(mid));
            case 2: return makeAssign(lo, makeBinOp(ExprKind::Sub, makeVar(mid), makeIntConst(1)));
            case 3: return makeAssign(hi, makeVar(mid));
            case 4: return makeAssign(hi, makeBinOp(ExprKind::Sub, makeVar(mid), makeIntConst(1)));
            case 5: return makeAssign(hi, makeBinOp(ExprKind::Add, makeVar(mid), makeIntConst(1)));
            case 6: return makeAssign(lo, makeBinOp(ExprKind::Add, makeVar(lo), makeIntConst(1)));
            default: return makeAssign(hi, makeBinOp(ExprKind::Sub, makeVar(hi), makeIntConst(1)));
        }
    };
    auto foundBody = [](int f) {
        std::vector<StmtPtr> body;
        body.push_back(makeAssign(7, makeIntConst(1)));
        body.push_back(f == 0 ? makeAssign(3, makeVar(2)) : makeAssign(2, makeVar(3)));
        return body;
    };
    std::vector<int> atoms = {0, 1, 2, 3, 4, 5, 6};  // n, target, lo, hi, mid, v, dir

    // Decision snapshots, traced from the two linear escapes (no knowledge
    // of the answer), subsampled 1 in 3.
    std::vector<std::vector<int>> snapshots;
    for (int escape : {6, 7}) {
        BinarySearchWithDecision open = buildBinarySearchWithDecision(skel, makeIntConst(1), move(escape), move(escape));
        Interpreter interp(stepLimit);
        long visits = 0;
        interp.setConditionTrace(open.decisionSite, [&](const std::vector<int>& v, const std::vector<int>&) {
            if (visits++ % 3 == 0 && snapshots.size() < 6000) snapshots.push_back(v);
        });
        std::vector<int> out;
        for (const auto& inst : training) interp.run(*open.program, inst.input, numVars, out);
    }
    std::vector<ExprPtr> all = enumerateConditions(atoms, 2, /*products=*/true);
    size_t enumerated = all.size();
    std::vector<ExprPtr> reps = dedupeByTruthVector(std::move(all), snapshots);

    // Stage 2: found-check candidates, from found-check states traced from
    // the same escapes with C1 = 0 (found never set), labeled by whether
    // the instance's target is absent.
    std::vector<ExprPtr> c1s;
    if (stage2) {
        std::vector<std::vector<int>> states;
        std::vector<char> absent;
        for (int escape : {6, 7}) {
            BinarySearchWithDecision open =
                buildBinarySearchStage2(skel, makeIntConst(0), foundBody(0), makeIntConst(1), move(escape), move(escape));
            Interpreter interp(stepLimit);
            char curAbsent = 0;
            interp.setConditionTrace(open.foundCheckSite, [&](const std::vector<int>& v, const std::vector<int>&) {
                states.push_back(v);
                absent.push_back(curAbsent);
            });
            std::vector<int> out;
            for (const auto& inst : training) {
                curAbsent = inst.expectedOutput[0] == 0 ? 1 : 0;
                interp.run(*open.program, inst.input, numVars, out);
            }
        }
        std::vector<ExprPtr> candidates = enumerateConditions(atoms, 2, /*products=*/true);
        size_t c1Enumerated = candidates.size();
        std::vector<ExprPtr> kept;
        for (auto& c : candidates) {
            bool ok = true, anyPresent = false;
            for (size_t k = 0; k < states.size() && ok; ++k) {
                if (evalOnSnapshot(*c, states[k]) == 0) continue;
                if (absent[k]) ok = false;
                else anyPresent = true;
            }
            if (ok && anyPresent) kept.push_back(std::move(c));
        }
        std::vector<std::vector<int>> sample;
        for (size_t k = 0; k < states.size(); k += 3) sample.push_back(states[k]);
        size_t keptCount = kept.size();
        c1s = dedupeByTruthVector(std::move(kept), sample);
        std::cout << "[bsearch] stage 2: " << c1Enumerated << " found-check conditions -> " << keptCount
                  << " consistent with the labels -> " << c1s.size() << " classes, x 2 found bodies\n";
    }
    std::cout << "[bsearch] " << training.size() << " training instances (n=2.." << args.maxN << "), " << snapshots.size()
              << " snapshots, " << enumerated << " decision conditions -> " << reps.size() << " classes, x 56 move pairs\n";

    struct Candidate {
        int c1 = -1, f = 0;  // stage 2 only
        size_t cls;
        int a, b;
        long steps;
        double growth = 0;
    };
    auto build = [&](const Candidate& c) {
        if (c.c1 < 0) return buildBinarySearchWithDecision(skel, reps[c.cls]->clone(), move(c.a), move(c.b));
        return buildBinarySearchStage2(skel, c1s[static_cast<size_t>(c.c1)]->clone(), foundBody(c.f), reps[c.cls]->clone(),
                                       move(c.a), move(c.b));
    };
    auto describe = [&](const Candidate& c) {
        std::string d = c.c1 < 0 ? "" : "if (" + c1s[static_cast<size_t>(c.c1)]->toString() + ") { " + foundNames[c.f] + " } else ";
        return d + "if (" + reps[c.cls]->toString() + ") { " + moveNames[c.a] + " } else { " + moveNames[c.b] + " }";
    };
    // Full evaluation (steps summed over all instances).
    auto evaluate = [&](const Stmt& prog, const std::vector<Instance>& insts, long& steps) {
        Interpreter interp(stepLimit);
        std::vector<int> out;
        steps = 0;
        for (const auto& inst : insts) {
            long st = 0;
            if (problem.evaluateInto(prog, inst, interp, out, &st) != ExecStatus::SUCCESS) return false;
            steps += st;
        }
        return true;
    };

    // Filtering with a per-thread "killer" order: the instance that rejected
    // the last candidate moves to the front, so most wrong candidates die on
    // their first run. Survivors are then re-evaluated in full for steps.
    unsigned workers = args.threads > 0 ? static_cast<unsigned>(args.threads) : std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::vector<Candidate>> found(workers);
    std::vector<int> c1Range = stage2 ? std::vector<int>{} : std::vector<int>{-1};
    for (size_t i = 0; stage2 && i < c1s.size(); ++i) c1Range.push_back(static_cast<int>(i));
    const int fCount = stage2 ? 2 : 1;
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < workers; ++t) {
        pool.emplace_back([&, t] {
            // A correct candidate needs a few hundred steps at n <= 12 even
            // when linear; non-terminating ones would otherwise burn the
            // full limit on every rejection.
            Interpreter interp(20'000);
            std::vector<int> out;
            std::vector<size_t> order(training.size());
            for (size_t k = 0; k < order.size(); ++k) order[k] = k;
            for (int c1 : c1Range) {
                for (int f = 0; f < fCount; ++f) {
                    for (size_t i = t; i < reps.size(); i += workers) {
                        for (int a = 0; a < 8; ++a) {
                            for (int b = 0; b < 8; ++b) {
                                if (a == b && i != 0) continue;  // the decision is irrelevant; evaluate once
                                Candidate cand{c1, f, i, a, b, 0};
                                auto prog = build(cand);
                                bool ok = true;
                                for (size_t k = 0; k < order.size(); ++k) {
                                    if (problem.evaluateInto(*prog.program, training[order[k]], interp, out) != ExecStatus::SUCCESS) {
                                        std::rotate(order.begin(), order.begin() + static_cast<long>(k), order.begin() + static_cast<long>(k) + 1);
                                        ok = false;
                                        break;
                                    }
                                }
                                if (ok) {
                                    evaluate(*prog.program, training, cand.steps);
                                    found[t].push_back(cand);
                                }
                            }
                        }
                    }
                }
            }
        });
    }
    for (auto& th : pool) th.join();
    std::vector<Candidate> correct;
    for (auto& f : found) correct.insert(correct.end(), f.begin(), f.end());
    std::sort(correct.begin(), correct.end(), [](const Candidate& x, const Candidate& y) {
        if (x.steps != y.steps) return x.steps < y.steps;
        if (x.c1 != y.c1) return x.c1 < y.c1;
        if (x.f != y.f) return x.f < y.f;
        return x.cls != y.cls ? x.cls < y.cls : x.a * 8 + x.b < y.a * 8 + y.b;
    });

    // Step growth n = 16 -> 128 on the benchmark instance, mean over 5 seeds.
    auto growth = [&](const Stmt& prog) {
        double s16 = 0, s128 = 0;
        Interpreter interp(stepLimit);
        std::vector<int> out;
        for (uint64_t seed = 1; seed <= 5; ++seed) {
            long a = 0, b = 0;
            if (problem.evaluateInto(prog, problem.generateBenchmarkInstance(16, seed), interp, out, &a) != ExecStatus::SUCCESS ||
                problem.evaluateInto(prog, problem.generateBenchmarkInstance(128, seed), interp, out, &b) != ExecStatus::SUCCESS) {
                return -1.0;
            }
            s16 += static_cast<double>(a);
            s128 += static_cast<double>(b);
        }
        return s128 / std::max(1.0, s16);
    };
    int bisection = 0, linear = 0;
    for (auto& c : correct) {
        c.growth = growth(*build(c).program);
        if (c.growth > 0 && c.growth <= 3) bisection++;
        if (c.growth >= 6) linear++;
    }
    std::cout << "[bsearch] " << correct.size() << " candidates correct on every training instance: " << bisection
              << " with growth <= 3 (bisection), " << linear << " with growth >= 6 (linear)\n";

    // Hand-written reference: v == target -> found; (v - target) * dir < 0 -> lo = mid + 1, else hi = mid.
    ExprPtr hand = makeCompare(ExprKind::Lt, makeBinOp(ExprKind::Mul, makeBinOp(ExprKind::Sub, makeVar(5), makeVar(1)), makeVar(6)),
                               makeIntConst(0));
    auto handProg = buildBinarySearchWithDecision(skel, hand->clone(), move(0), move(3));
    long handSteps = 0;
    bool handOk = evaluate(*handProg.program, training, handSteps);
    size_t handRank = 0;
    for (const auto& c : correct) handRank += c.steps < handSteps ? 1 : 0;
    std::cout << "[bsearch] hand-written program: " << (handOk ? "correct" : "SANITY FAILURE") << ", " << handSteps
              << " training steps, growth x" << std::fixed << std::setprecision(2) << growth(*handProg.program)
              << ", would rank #" << (handRank + 1) << "\n";
    std::cout << "[bsearch] fastest correct candidates (v1 target, v2 lo, v3 hi, v4 mid, v5 v, v6 dir):\n";
    for (size_t k = 0; k < correct.size() && k < 12; ++k) {
        const auto& c = correct[k];
        std::cout << "  " << std::setw(9) << c.steps << " steps  growth x" << std::setprecision(2) << c.growth << "  " << describe(c)
                  << "\n";
    }
    if (correct.empty()) return;
    // Held-out for every candidate tied with the winner on training steps:
    // the tie-break among them is enumeration order, not evidence.
    std::vector<Instance> heldOut;
    for (int n = args.maxN + 1; n <= args.maxN + 8; ++n) {
        for (auto& inst : problem.generateInstances(n)) heldOut.push_back(std::move(inst));
        for (auto& inst : problem.generateAdversarialInstances(n)) heldOut.push_back(std::move(inst));
    }
    for (const auto& c : correct) {
        if (c.steps != correct.front().steps) break;
        long st = 0;
        bool ok = evaluate(*build(c).program, heldOut, st);
        std::cout << "[bsearch] held-out n=" << (args.maxN + 1) << ".." << (args.maxN + 8) << " (" << heldOut.size()
                  << " instances): " << (ok ? "correct" : "WRONG") << "  " << describe(c) << "\n";
    }
}

int main(int argc, char** argv) {
    CliArgs args;
    if (!parseArgs(argc, argv, args)) {
        printUsage();
        return 1;
    }
    resolveNumVars(args);
    // Section 94: with restarts the seed is only the master of a seed
    // stream, so an unset one is drawn at random -- and printed, so the run
    // can still be replayed with --seed.
    if (args.restartOnStall > 0 && !args.seedGiven) {
        args.seed = (static_cast<uint64_t>(std::random_device{}()) << 32) | std::random_device{}();
        std::cout << "[seed] master seed " << args.seed << " (random; replay with --seed " << args.seed << ")\n";
    }

    std::unique_ptr<Problem> problem = makeProblem(args.problem, args.numVars, args.seed);
    if (!problem) {
        std::cerr << "Unknown problem: " << args.problem
                   << " (available: sorting, min, max, sum, contains, contains_ordered, is_prime, gcd, mod, factorial, fibonacci, "
                      "count, average, reverse, power, three_sum, single_number, graph_reachability, shortest_path, tsp, dtw, "
                      "lis, substring, cycle, dominated_window)\n";
        return 1;
    }

    if (!args.ablateSkeleton.empty()) {
        unsigned threads = args.threads > 0 ? static_cast<unsigned>(args.threads) : std::max(1u, std::thread::hardware_concurrency());
        runSkeletonAblation(args.ablateSkeleton, *problem, args.startLevel, args.maxN, args.numVars,
                            static_cast<uint64_t>(args.seed), threads);
        return 0;
    }
    if (args.fillBsearchHole) {
        runFillBsearchHoleMode(args);
        return 0;
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
    config.behaviorNicheRate = args.behaviorNicheRate;
    config.speedBonusWeight = args.speedBonusWeight;
    config.postSolveSizes = args.postSolveSizes;
    config.tailEnumTop = args.tailEnumTop;
    config.regressionGuard = args.regressionGuard;
    config.genConfig.loopAccumulatorBias = args.loopBias;
    config.genConfig.compositeBias = args.compositeBias;
    config.genConfig.pruneGeneration = args.pruneGeneration;
    config.genConfig.roleTypedVars = args.roleTypedVars;
    config.genConfig.combinatorBias = args.combinatorBias;
    config.genConfig.combinatorAdjacent = args.combinatorAdjacent;
    config.genConfig.combinatorStep = args.combinatorStep;
    config.genConfig.combinatorState = args.combinatorState;
    config.genConfig.combinatorNested = args.combinatorNested;
    config.genConfig.nestedCanonicalTests = args.nestedCanonicalTests;
    config.genConfig.combinatorFrequency = args.combinatorFrequency;
    config.genConfig.combinatorFollow = args.combinatorFollow;
    config.genConfig.followPointerInputsOnly = args.followPointerInputsOnly;
    config.genConfig.followExtended = args.followExtended;
    config.genConfig.scalarLoop = args.scalarLoop;
    config.genConfig.scalarLoopIndexRange = args.scalarLoopIndexRange;
    config.genConfig.nestedPlace = args.nestedPlace;
    config.genConfig.nestedPlaceOption = args.nestedPlaceOption;
    config.genConfig.combinatorTriple = args.combinatorTriple;
    config.genConfig.combinatorRelax = args.combinatorRelax;
    config.genConfig.combinatorGrid = args.combinatorGrid;
    config.genConfig.combinatorOrthogonal = args.combinatorOrthogonal;
    config.genConfig.combinatorArgBest = args.combinatorArgBest;
    config.genConfig.combinatorDistinct = args.combinatorDistinct;
    config.genConfig.adaptiveSkeletonBiasRate = args.adaptiveSkeletonBiasRate;
    config.genConfig.secondArrayBias = args.secondArrayBias;
    config.genConfig.recursionCallBias = args.recursionCallBias;
    config.genConfig.dcShellBias = args.dcShellBias;
    config.genConfig.dcShellSearchBias = args.dcShellSearchBias;
    config.genConfig.dcShellSearchKey = args.dcShellSearchKey;
    config.genConfig.dcShellSearchEnd = args.dcShellSearchEnd;
    config.adaptiveBiasLogPath = args.adaptiveBiasLog;
    config.checkpointPath = args.checkpointPath;
    config.restartOnStall = args.restartOnStall;
    config.wallSeconds = args.wallSeconds;
    config.profileNovelty = args.profileNovelty;
    config.profileBan = args.profileBan;
    config.forbidStmtMask = args.forbidStmtMask;
    std::unique_ptr<Problem> confirmProblem;
    if (args.confirmReseed > 0) {
        confirmProblem = makeProblem(args.problem, args.numVars, args.seed ^ 0x5bd1e995ULL);
        config.confirmProblem = confirmProblem.get();
        config.confirmReseedLevels = args.confirmReseed;
    }

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
