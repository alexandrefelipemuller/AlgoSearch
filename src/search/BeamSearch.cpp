#include "search/BeamSearch.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <array>
#include <map>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "search/Dedup.hpp"
#include "search/Logger.hpp"
#include "search/TailFiller.hpp"

namespace algosearch {

namespace {

// Number of score deciles used by score-band niching (SearchConfig::scoreBandRate).
constexpr int kScoreBands = 10;

// Stable, short id for a program: a hex hash of its canonical text. Every
// per-N row produced by one promotion cascade shares this id, since they
// all describe the exact same unmutated program.
std::string programIdOf(const std::string& programText) {
    std::ostringstream oss;
    oss << std::hex << std::hash<std::string>{}(programText);
    return oss.str();
}

// Variable-renaming-insensitive dedup: see ast/Stmt.hpp's canonicalText()
// doc comment and FINDINGS.md section 29 for why programIdOf/the divergence
// window's "genuinely new champion" check use this instead of raw text.

// A coarse "shape" signature: counts of each statement kind plus the
// deepest while-nesting, ignoring literal constants and variable/array
// indices entirely. Two programs that differ only in which constant or
// variable they poke are the same shape here; two programs with a
// different number of loops or branches are not. Used only to measure
// beam diversity (how many distinct shapes survive to the moment a full
// solve is found) -- never for scoring or selection.
void tallyShape(const Stmt& s, int depth, int counts[6], int& maxDepth) {
    switch (s.kind) {
        case StmtKind::Assign: counts[0]++; break;
        case StmtKind::AssignArray: counts[1]++; break;
        case StmtKind::If: counts[2]++; break;
        case StmtKind::While: counts[3]++; break;
        case StmtKind::Swap: counts[4]++; break;
        case StmtKind::Return: counts[5]++; break;
        case StmtKind::Block:
        case StmtKind::Continue:
        case StmtKind::Break:
            break;
    }
    int childDepth = depth + (s.kind == StmtKind::While ? 1 : 0);
    maxDepth = std::max(maxDepth, childDepth);
    for (const auto& c : s.body) tallyShape(*c, childDepth, counts, maxDepth);
    for (const auto& c : s.elseBody) tallyShape(*c, childDepth, counts, maxDepth);
}

std::string structuralShape(const Stmt& program) {
    int counts[6] = {0, 0, 0, 0, 0, 0};
    int maxDepth = 0;
    tallyShape(program, 0, counts, maxDepth);
    std::ostringstream oss;
    for (int c : counts) oss << c << ",";
    oss << maxDepth;
    return oss.str();
}

// Detects the atomic counting-loop skeleton (finding #7 of FINDINGS.md:
// init + bound + increment as ONE coordinated unit, not any ingredient in
// isolation -- a falsification test showed upweighting the increment idiom
// alone never rescues a stuck problem). Mirrors exactly what
// genCountingLoopBlock() in ProgramGenerator.cpp emits: within some block,
// an `vi = 0` assignment followed later by a `while (vi < var0) { ...;
// vi = vi + 1; }`. Used only to drive the adaptive-bias experiment
// (GenConfig::adaptiveSkeletonBiasRate) -- never affects scoring/selection.
bool blockHasSelfIncrement(const std::vector<StmtPtr>& body, int vi) {
    for (const auto& s : body) {
        if (s->kind == StmtKind::Assign && s->varIndex == vi && s->valueExpr &&
            s->valueExpr->kind == ExprKind::Add && s->valueExpr->lhs && s->valueExpr->rhs &&
            s->valueExpr->lhs->kind == ExprKind::Var && s->valueExpr->lhs->varIndex == vi &&
            s->valueExpr->rhs->kind == ExprKind::IntConst && s->valueExpr->rhs->intValue == 1) {
            return true;
        }
    }
    return false;
}

bool isCountingWhile(const Stmt& s, int vi) {
    if (s.kind != StmtKind::While || !s.cond || s.cond->kind != ExprKind::Lt) return false;
    const Expr& lhs = *s.cond->lhs;
    const Expr& rhs = *s.cond->rhs;
    if (lhs.kind != ExprKind::Var || lhs.varIndex != vi) return false;
    if (rhs.kind != ExprKind::Var || rhs.varIndex != 0) return false;
    return blockHasSelfIncrement(s.body, vi);
}

bool blockHasSkeleton(const std::vector<StmtPtr>& body) {
    for (size_t i = 0; i < body.size(); ++i) {
        const Stmt& s = *body[i];
        if (s.kind != StmtKind::Assign || s.varIndex == 0 || !s.valueExpr) continue;
        if (s.valueExpr->kind != ExprKind::IntConst || s.valueExpr->intValue != 0) continue;
        int vi = s.varIndex;
        for (size_t j = i + 1; j < body.size(); ++j) {
            if (isCountingWhile(*body[j], vi)) return true;
        }
    }
    return false;
}

bool hasCountingLoopSkeleton(const Stmt& s) {
    if ((s.kind == StmtKind::Block || s.kind == StmtKind::If || s.kind == StmtKind::While) && blockHasSkeleton(s.body)) {
        return true;
    }
    if (s.kind == StmtKind::If && blockHasSkeleton(s.elseBody)) return true;
    for (const auto& c : s.body) {
        if (hasCountingLoopSkeleton(*c)) return true;
    }
    for (const auto& c : s.elseBody) {
        if (hasCountingLoopSkeleton(*c)) return true;
    }
    return false;
}

struct CandidateRecord {
    Program program;
    int size = 0;
    int score = 0;       // exact instances solved -- the only thing ever used for
                          // promotion decisions and reported as "score" in logs/records
    // Diagnostic only (ALGOSEARCH_DEBUG_BEHAVIOR, FINDINGS.md section 35):
    // hash of the exact (status, output array) across ALL instances, and
    // across only the first behaviorPrefixInstances -- the full one defines
    // "behaviorally identical", the prefix one is the cheap pre-filter a
    // generation-time behavioral dedup would use.
    size_t behaviorHashFull = 0;
    size_t behaviorHashPrefix = 0;
    bool behaviorAnyFailure = false;  // some instance hit TIMEOUT or RUNTIME_ERROR
    double fitness = 0;  // score + inversions-based partial credit; search-internal
                          // ranking heuristic only, never substitutes for `score`
    int totalTests = 0;
    // Which instances this candidate got exactly right, in instance
    // order -- a candidate's BEHAVIOR, not its structure. Used for
    // behavior-niche selection (see SearchConfig::behaviorNicheRate):
    // deliberately problem-agnostic (never looks at what the "right"
    // answer means, just which instances matched), so rewarding a
    // different pattern here is never a disguised way of telling the
    // search what a specific problem's correct algorithm looks like --
    // unlike a hand-picked "reward this AST shape" bonus would be.
    std::vector<bool> correctness;
};

// A candidate's behavior signature: its correctness pattern encoded as a
// string, for use as a niche key exactly like structuralShape() is for
// shapeNicheRate -- two candidates with the SAME right/wrong pattern
// across every instance are behaviorally redundant even if their code
// looks completely different; two candidates that differ on which
// instances they get right represent genuinely different partial
// progress, worth keeping both around for.
std::string behaviorSignature(const CandidateRecord& rec) {
    std::string sig;
    sig.reserve(rec.correctness.size());
    for (bool b : rec.correctness) sig.push_back(b ? '1' : '0');
    return sig;
}

// Scores every candidate in `pool` against `instances`, writing results into
// `scored` starting at `outOffset` (one CandidateRecord per candidate, same
// order). Splits the work across threads: each candidate is evaluated fully
// independently (Interpreter::run and Problem methods touch no shared
// mutable state), so this is embarrassingly parallel and needs no locking.
constexpr size_t behaviorPrefixInstances = 3;

void mixHash(size_t& h, size_t v) { h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2); }

void scoreCandidates(std::vector<Program>& pool, const std::vector<Instance>& instances, const Problem& problem,
                      const Interpreter& interp, unsigned numThreads, std::vector<CandidateRecord>& scored, size_t outOffset,
                      int& timeouts, int& errors, bool trackCorrectness = false, double speedBonusWeight = 0.0,
                      long stepLimit = 1, bool trackBehaviorHash = false) {
    size_t n = pool.size();
    for (size_t i = 0; i < n; ++i) {
        scored[outOffset + i].program = std::move(pool[i]);
        scored[outOffset + i].totalTests = static_cast<int>(instances.size());
    }
    if (n == 0) return;

    unsigned workers = std::max(1u, numThreads);
    workers = std::min<unsigned>(workers, static_cast<unsigned>(n));
    std::vector<int> timeoutsPerThread(workers, 0), errorsPerThread(workers, 0);
    size_t chunk = (n + workers - 1) / workers;

    std::vector<std::thread> threads;
    threads.reserve(workers);
    for (unsigned t = 0; t < workers; ++t) {
        size_t begin = t * chunk;
        size_t end = std::min(n, begin + chunk);
        if (begin >= end) continue;
        threads.emplace_back([&, begin, end, t]() {
            std::vector<int> output;  // reused across every run in this worker
            for (size_t i = begin; i < end; ++i) {
                CandidateRecord& rec = scored[outOffset + i];
                int score = 0;
                double fitness = 0.0;
                if (trackCorrectness) rec.correctness.reserve(instances.size());
                size_t instIndex = 0;
                for (const auto& inst : instances) {
                    long steps = 0;
                    ExecStatus status = problem.evaluateInto(*rec.program, inst, interp, output,
                                                              speedBonusWeight > 0.0 ? &steps : nullptr);
                    if (status == ExecStatus::SUCCESS) {
                        score++;
                        fitness += 1.0;
                        if (speedBonusWeight > 0.0 && stepLimit > 0) {
                            double frac = std::clamp(static_cast<double>(steps) / static_cast<double>(stepLimit), 0.0, 1.0);
                            fitness += speedBonusWeight * (1.0 - frac);
                        }
                    } else if (status == ExecStatus::TIMEOUT) {
                        timeoutsPerThread[t]++;
                    } else if (status == ExecStatus::RUNTIME_ERROR) {
                        errorsPerThread[t]++;
                    } else {
                        fitness += problem.partialScore(inst.input, output);
                    }
                    if (trackCorrectness) rec.correctness.push_back(status == ExecStatus::SUCCESS);
                    if (trackBehaviorHash) {
                        // TIMEOUT/RUNTIME_ERROR outputs are partial garbage:
                        // hash only the status for those, so two programs that
                        // both crash count as behaving the same.
                        size_t h = static_cast<size_t>(status);
                        if (status == ExecStatus::TIMEOUT || status == ExecStatus::RUNTIME_ERROR) rec.behaviorAnyFailure = true;
                        if (status != ExecStatus::TIMEOUT && status != ExecStatus::RUNTIME_ERROR) {
                            for (int v : output) mixHash(h, std::hash<int>{}(v));
                        }
                        mixHash(rec.behaviorHashFull, h);
                        if (instIndex < behaviorPrefixInstances) mixHash(rec.behaviorHashPrefix, h);
                    }
                    instIndex++;
                }
                rec.size = rec.program->size();
                rec.score = score;
                rec.fitness = fitness;
            }
        });
    }
    for (auto& th : threads) th.join();

    for (unsigned t = 0; t < workers; ++t) {
        timeouts += timeoutsPerThread[t];
        errors += errorsPerThread[t];
    }
}

std::vector<std::pair<std::string, std::string>> configToPairs(const SearchConfig& cfg) {
    return {
        {"beam_width", std::to_string(cfg.beamWidth)},
        {"max_program_size", std::to_string(cfg.maxProgramSize)},
        {"step_limit", std::to_string(cfg.stepLimit)},
        {"start_level", std::to_string(cfg.startLevel)},
        {"max_level", std::to_string(cfg.maxLevel)},
        {"seed", std::to_string(cfg.seed)},
        {"candidates_per_size", std::to_string(cfg.candidatesPerSize)},
        {"post_solve_sizes", std::to_string(cfg.postSolveSizes)},
        {"loop_bias", std::to_string(cfg.genConfig.loopAccumulatorBias)},
        {"adaptive_skeleton_bias_rate", std::to_string(cfg.genConfig.adaptiveSkeletonBiasRate)},
    };
}

}  // namespace

BeamSearch::BeamSearch(const Problem& problem, SearchConfig config) : problem_(problem), config_(std::move(config)) {}

std::vector<SolutionRecord> BeamSearch::run(std::vector<Program>* outPrograms) {
    Random rng(config_.seed);
    Interpreter interp(config_.stepLimit, /*maxRecursionDepth=*/200, config_.genConfig.recursionCallBias > 0.0);
    Dedup dedup;

    std::vector<CandidateRecord> beam;
    int currentLevel = config_.startLevel;
    std::vector<SolutionRecord> solutions;

    // Simulated-annealing walker: a SINGLE persistent solution, mutated
    // one local step at a time each round (never a fresh random restart --
    // that's what diversityRate already does), and accepted as the new
    // current state even when it scores WORSE, with Boltzmann probability
    // exp(delta/temperature). This is the one thing neither diversityRate
    // (random immigrants, no relation to current progress) nor
    // shapeNicheRate (keeps only the best of each shape, never accepts a
    // worse move) actually does: a controlled downhill walk that can cross
    // a fitness valley to escape a local optimum, the way real SA does.
    // Runs independently of beam selection -- its current candidate is
    // still injected into the same scored pool each round, so if it ever
    // becomes competitive, the elitist beam can adopt it too. Temperature
    // cools linearly over the size sweep (config_.annealingTemperature at
    // size=1, down to a small floor as size approaches maxProgramSize).
    Program annealState;
    double annealFitness = -1.0;

    auto searchStart = std::chrono::steady_clock::now();

    // Throughput profiling: coarse wall-clock split of where each round's
    // time actually goes, plus a running count of candidates
    // generated/scored -- diagnostic only, never affects search behavior.
    // Printed once at the end of run().
    double genPhaseSeconds = 0.0;
    double scorePhaseSeconds = 0.0;
    double sortPhaseSeconds = 0.0;
    long totalCandidatesGenerated = 0;
    long totalCandidatesScored = 0;

    std::ofstream adaptiveLog;
    bool adaptiveBiasOn = config_.genConfig.adaptiveSkeletonBiasRate > 0.0;
    if (adaptiveBiasOn && !config_.adaptiveBiasLogPath.empty()) {
        adaptiveLog.open(config_.adaptiveBiasLogPath);
        adaptiveLog << "round,level,bias_before,skeleton_rate,bias_after,best_score\n";
    }

    // Moção 1 (Etapa 2): divergence-window state. See SearchConfig::postSolveSizes.
    bool postSolveWindowOpen = false;
    bool postSolveWindowOpenedThisSize = false;
    int postSolveRemaining = 0;
    int postSolvePendingAdvance = 0;
    int postSolveBestCascadeLevel = 0;
    int postSolveBestSize = 0;
    // Only meaningful when config_.speedBonusWeight > 0.0 (see the divergence
    // window's promotion tie-break below) -- the archived champion's fitness
    // AT THE MOMENT its cascade was measured, i.e. score + speed bonus, so a
    // same-reach alternative that runs faster (fewer interpreter steps) can
    // win promotion even though it's a bigger program. Without this, the
    // window's only tie-break was program size, which always favors the
    // smaller-but-slower candidate no matter how much speedBonusWeight
    // rewards speed between coexisting candidates (see FINDINGS.md section
    // 23: a dedicated skeleton correct-but-bigger than a trivial scan never
    // had a mechanism to win even once both existed in the same window).
    double postSolveBestFitness = 0.0;
    // ALGOSEARCH_DEBUG_BEHAVIOR tallies, per level, per origin
    // (0 = mutant of a beam survivor, 1 = fresh random, 2 = crossover).
    struct BehaviorTally {
        long candidates = 0, dupFull = 0, dupFullZeroScore = 0, dupPrefix = 0, prefixFalseMerge = 0, neutral = 0;
        long newClean = 0;  // new behavior with no TIMEOUT/RUNTIME_ERROR on any instance
    };
    std::map<int, std::array<BehaviorTally, 3>> behaviorTallies;
    std::unordered_set<std::string> frozenChampions;
    std::unordered_set<std::string> recordedKeys;  // "programId@levelReached"
    std::map<int, std::vector<Instance>> seenInstancesByLevel;  // tail enumeration (section 51)

    for (int size = 1; size <= config_.maxProgramSize && currentLevel <= config_.maxLevel; ++size) {
        postSolveWindowOpenedThisSize = false;
        std::vector<Instance> instances = problem_.generateInstances(currentLevel);
        log::levelHeader(currentLevel, problem_.describeLevel(currentLevel), instances.size(), config_.beamWidth);

        // Build the candidate pool: grow each beam survivor to this size,
        // then top up with fresh random programs of exactly this size.
        // Beam survivors are grown towards the new target size and always
        // rescored, regardless of dedup: growProgram may leave a program
        // unchanged when it already meets/exceeds `size`, which would
        // otherwise look like an already-seen duplicate and cause the beam
        // to silently lose its best candidate between iterations. Dedup is
        // only used to avoid rescoring duplicates among freshly generated
        // random candidates.
        // Elitism: keep an unmutated clone of every beam survivor alongside
        // a grown/mutated variant, so a point-mutation that makes things
        // worse can never cause the beam's current best to be lost.
        auto genStart = std::chrono::steady_clock::now();
        std::vector<Program> grownPool;
        grownPool.reserve(beam.size() * 2);
        for (auto& b : beam) {
            grownPool.push_back(b.program->clone());
            grownPool.push_back(growProgram(*b.program, size, rng, config_.genConfig));
        }

        // Section 51: for the top beam survivors, cut the program after each
        // of its last top-level statements and append the best enumerated
        // tail (search/TailFiller.hpp). The tail is chosen on the instances of
        // EVERY level seen so far, not just this one: a small-n guard
        // (e.g. n = 1) only earns its keep on the early levels.
        if (config_.tailEnumTop > 0 && !beam.empty()) {
            std::vector<Instance> seen;
            for (int l = config_.startLevel; l <= currentLevel; l = problem_.nextLevel(l)) {
                auto it = seenInstancesByLevel.find(l);
                if (it == seenInstancesByLevel.end()) it = seenInstancesByLevel.emplace(l, problem_.generateInstances(l)).first;
                seen.insert(seen.end(), it->second.begin(), it->second.end());
            }
            int top = std::min(config_.tailEnumTop, static_cast<int>(beam.size()));
            for (int b = 0; b < top; ++b) {
                const Stmt& program = *beam[static_cast<size_t>(b)].program;
                int statements = static_cast<int>(program.body.size());
                for (int cut = std::max(0, statements - 2); cut <= statements; ++cut) {
                    auto prefix = makeBlock();
                    for (int k = 0; k < cut; ++k) prefix->body.push_back(program.body[static_cast<size_t>(k)]->clone());
                    if (auto filled = fillTail(*prefix, seen, problem_, interp)) grownPool.push_back(std::move(filled));
                }
            }
        }

        std::vector<Program> freshPool;
        int guard = 0;
        while (static_cast<int>(freshPool.size()) < config_.candidatesPerSize && guard < config_.candidatesPerSize * 4) {
            auto candidate = generateRandomProgram(rng, size, config_.genConfig);
            if (dedup.insertIfNew(*candidate)) freshPool.push_back(std::move(candidate));
            guard++;
        }

        // Crossover pool: two DISTINCT beam survivors combined via
        // subtree splicing (see SearchConfig::crossoverRate). Needs at
        // least 2 survivors to have two different parents to draw from.
        std::vector<Program> crossoverPool;
        if (config_.crossoverRate > 0.0 && beam.size() >= 2) {
            int crossoverCount = std::max(0, static_cast<int>(std::lround(config_.crossoverRate * config_.candidatesPerSize)));
            crossoverPool.reserve(static_cast<size_t>(crossoverCount));
            for (int c = 0; c < crossoverCount; ++c) {
                int ia = rng.uniformInt(0, static_cast<int>(beam.size()) - 1);
                int ib = rng.uniformInt(0, static_cast<int>(beam.size()) - 1);
                if (ib == ia) ib = (ib + 1) % static_cast<int>(beam.size());
                crossoverPool.push_back(crossoverPrograms(*beam[static_cast<size_t>(ia)].program,
                                                            *beam[static_cast<size_t>(ib)].program, rng, config_.genConfig));
            }
        }
        // Annealer pool: one mutation of the walker's current state (or a
        // fresh random program on its very first round). Scored the same
        // as everything else; acceptance is decided after scoring, below.
        std::vector<Program> annealPool;
        if (config_.annealingTemperature > 0.0) {
            annealPool.push_back(annealState ? growProgram(*annealState, size, rng, config_.genConfig)
                                              : generateRandomProgram(rng, size, config_.genConfig));
        }

        totalCandidatesGenerated += guard + static_cast<int>(crossoverPool.size()) + static_cast<int>(annealPool.size());
        genPhaseSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - genStart).count();

        int timeouts = 0, errors = 0;
        unsigned threadCount = config_.numThreads != 0 ? config_.numThreads : std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 1;

        bool debugLandscape = std::getenv("ALGOSEARCH_DEBUG_LANDSCAPE") != nullptr;
        bool trackCorrectness = config_.behaviorNicheRate > 0.0 || debugLandscape;
        bool debugBehavior = std::getenv("ALGOSEARCH_DEBUG_BEHAVIOR") != nullptr;
        auto scoreStart = std::chrono::steady_clock::now();
        std::vector<CandidateRecord> scored(grownPool.size() + freshPool.size() + crossoverPool.size() + annealPool.size());
        scoreCandidates(grownPool, instances, problem_, interp, threadCount, scored, 0, timeouts, errors, trackCorrectness,
                         config_.speedBonusWeight, config_.stepLimit, debugBehavior);
        scoreCandidates(freshPool, instances, problem_, interp, threadCount, scored, grownPool.size(), timeouts, errors,
                         trackCorrectness, config_.speedBonusWeight, config_.stepLimit, debugBehavior);
        scoreCandidates(crossoverPool, instances, problem_, interp, threadCount, scored, grownPool.size() + freshPool.size(),
                         timeouts, errors, trackCorrectness, config_.speedBonusWeight, config_.stepLimit, debugBehavior);
        size_t annealIndex = grownPool.size() + freshPool.size() + crossoverPool.size();
        scoreCandidates(annealPool, instances, problem_, interp, threadCount, scored, annealIndex, timeouts, errors,
                         trackCorrectness, config_.speedBonusWeight, config_.stepLimit, debugBehavior);
        totalCandidatesScored += static_cast<long>(scored.size());

        // Diagnostic only (FINDINGS.md section 35, idea E's go/no-go): how
        // many candidates scored this size step behave EXACTLY like one
        // already in the beam or already scored earlier this step -- i.e.
        // how much of the evaluation budget a behavioral dedup could
        // reclaim -- and how well a cheap first-3-instances signature would
        // find them. grownPool is interleaved [elite clone, mutant], so each
        // mutant's parent behavior is known; a mutant identical to it is a
        // neutral mutation.
        if (debugBehavior) {
            std::unordered_set<size_t> seenFull, seenPrefix;
            for (size_t i = 0; i < grownPool.size(); i += 2) {
                seenFull.insert(scored[i].behaviorHashFull);
                seenPrefix.insert(scored[i].behaviorHashPrefix);
            }
            auto tally = [&](size_t i, int origin) {
                const CandidateRecord& r = scored[i];
                BehaviorTally& t = behaviorTallies[currentLevel][origin];
                t.candidates++;
                bool dupFull = !seenFull.insert(r.behaviorHashFull).second;
                bool dupPrefix = !seenPrefix.insert(r.behaviorHashPrefix).second;
                if (dupFull) t.dupFull++;
                if (!dupFull && !r.behaviorAnyFailure) t.newClean++;
                if (dupFull && r.score == 0) t.dupFullZeroScore++;
                if (dupPrefix) t.dupPrefix++;
                if (dupPrefix && !dupFull) t.prefixFalseMerge++;
            };
            for (size_t i = 1; i < grownPool.size(); i += 2) {
                if (scored[i].behaviorHashFull == scored[i - 1].behaviorHashFull) behaviorTallies[currentLevel][0].neutral++;
                tally(i, 0);
            }
            for (size_t i = grownPool.size(); i < grownPool.size() + freshPool.size(); ++i) tally(i, 1);
            for (size_t i = grownPool.size() + freshPool.size(); i < annealIndex; ++i) tally(i, 2);
        }
        scorePhaseSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - scoreStart).count();

        if (debugLandscape) {
            // Root-cause diagnostic: is the search plateau a BEAM
            // MONOCULTURE (the exact same lucky candidate cloned/mutated
            // every round, crowding out everything else) or a WIDE
            // FITNESS PLATEAU (many genuinely different programs
            // independently landing near the same score ceiling, so no
            // amount of diversity would find a path past it)? Distinguish
            // by comparing: (a) the best score reachable from freshPool
            // ALONE (brand-new independent samples, never descended from
            // the beam) vs from grownPool (mutations of current
            // survivors) -- if fresh sampling regularly finds candidates
            // near the ceiling too, the ceiling isn't a beam-capture
            // artifact; (b) how many DISTINCT behavior signatures
            // (right/wrong pattern across instances) appear among
            // near-ceiling candidates -- 1 means literally one family
            // dominates, >1 means multiple independent roads reach the
            // same score.
            int freshBest = 0, grownBest = 0;
            for (size_t i = 0; i < grownPool.size() && i < scored.size(); ++i) grownBest = std::max(grownBest, scored[i].score);
            for (size_t i = grownPool.size(); i < grownPool.size() + freshPool.size() && i < scored.size(); ++i) {
                freshBest = std::max(freshBest, scored[i].score);
            }
            int overallBest = 0;
            for (const auto& c : scored) overallBest = std::max(overallBest, c.score);
            int nearCeiling = std::max(0, static_cast<int>(overallBest * 0.9));
            std::unordered_set<std::string> distinctBehaviorsNearCeiling;
            std::unordered_map<int, int> scoreHistogram;
            for (const auto& c : scored) {
                scoreHistogram[c.score]++;
                if (c.score >= nearCeiling && !c.correctness.empty()) distinctBehaviorsNearCeiling.insert(behaviorSignature(c));
            }
            // Top few score buckets by candidate count, for a compact view
            // of where the mass of the population actually sits.
            std::vector<std::pair<int, int>> hist(scoreHistogram.begin(), scoreHistogram.end());
            std::sort(hist.begin(), hist.end(), [](auto& a, auto& b) { return a.first > b.first; });
            std::ostringstream histStr;
            for (size_t i = 0; i < hist.size() && i < 6; ++i) histStr << hist[i].first << ":" << hist[i].second << " ";
            std::cerr << "LANDSCAPE size=" << size << " overallBest=" << overallBest << " grownBest=" << grownBest
                      << " freshBest=" << freshBest << " distinctBehaviorsNear" << nearCeiling << "="
                      << distinctBehaviorsNearCeiling.size() << " topScores[score:count]=" << histStr.str() << "\n";
        }

        if (!annealPool.empty()) {
            // Boltzmann acceptance: always take an improving/equal move;
            // take a worsening one anyway with probability
            // exp(delta/temperature), delta normalized by instance count
            // so the configured temperature means roughly the same thing
            // regardless of how many instances this level has. Rejecting
            // simply leaves annealState as-is (the walker stays put, tries
            // a different local mutation from the same spot next round) --
            // real SA behavior, never a hard reset.
            const CandidateRecord& candidate = scored[annealIndex];
            double normalizedFitness = static_cast<double>(instances.size()) > 0
                                            ? candidate.fitness / static_cast<double>(instances.size())
                                            : candidate.fitness;
            double prevNormalized = annealFitness;
            double temperature =
                config_.annealingTemperature * (1.0 - static_cast<double>(size) / static_cast<double>(std::max(1, config_.maxProgramSize)));
            temperature = std::max(temperature, 0.02);
            bool accept = !annealState || normalizedFitness >= prevNormalized ||
                          rng.chance(std::exp((normalizedFitness - prevNormalized) / temperature));
            if (accept) {
                annealState = candidate.program->clone();
                annealFitness = normalizedFitness;
            }
        }

        if (scored.empty()) {
            log::progress(0, 0, static_cast<int>(instances.size()), 0, timeouts, errors);
            continue;
        }

        // Rank primarily by exact solved-count (the only metric that ever
        // decides promotion), then by inversions-based partial fitness as a
        // gradient when counts tie (crucial when every candidate solves 0
        // instances and a binary score gives the beam nothing to climb),
        // then prefer smaller programs to keep the beam diverse/simple.
        auto sortStart = std::chrono::steady_clock::now();
        std::sort(scored.begin(), scored.end(), [](const CandidateRecord& a, const CandidateRecord& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.fitness != b.fitness) return a.fitness > b.fitness;
            return a.size < b.size;
        });
        sortPhaseSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - sortStart).count();

        int bestScore = scored.front().score;
        int bestSize = scored.front().size;
        log::progress(scored.size(), bestScore, static_cast<int>(instances.size()), bestSize, timeouts, errors);

        // Diagnostic only (FINDINGS.md section 33's phase-3 go/no-go check):
        // how much of the current best candidate's size is provably dead by
        // canonicalText's own fold+DCE definition (see
        // rawAndSimplifiedSize's doc comment), tracked over time to see
        // whether a stalled level's bloat is removable junk or live code the
        // search is actually relying on. Every 20 size-steps, never gated
        // behind config so a run doesn't need to be relaunched to turn it on.
        if (std::getenv("ALGOSEARCH_DEBUG_BLOAT") != nullptr && size % 20 == 0 && scored.front().program) {
            auto [rawSz, simplifiedSz] = rawAndSimplifiedSize(*scored.front().program);
            std::cerr << "[bloat-debug] level=" << currentLevel << " size_step=" << size << " best_score=" << bestScore
                       << "/" << instances.size() << " raw=" << rawSz << " simplified=" << simplifiedSz
                       << " dead_frac=" << (rawSz > 0 ? 1.0 - static_cast<double>(simplifiedSz) / rawSz : 0.0)
                       << "\n";
        }

        int immigrantCount = 0;
        int eliteCount = config_.beamWidth;
        if (config_.diversityRate > 0.0) {
            immigrantCount = static_cast<int>(std::lround(config_.diversityRate * config_.beamWidth));
            immigrantCount = std::clamp(immigrantCount, 0, config_.beamWidth);
            eliteCount = config_.beamWidth - immigrantCount;
        }

        int shapeNicheCount = 0;
        if (config_.shapeNicheRate > 0.0) {
            shapeNicheCount = static_cast<int>(std::lround(config_.shapeNicheRate * config_.beamWidth));
            shapeNicheCount = std::clamp(shapeNicheCount, 0, eliteCount);
        }
        int behaviorNicheCount = 0;
        if (config_.behaviorNicheRate > 0.0) {
            behaviorNicheCount = static_cast<int>(std::lround(config_.behaviorNicheRate * config_.beamWidth));
            behaviorNicheCount = std::clamp(behaviorNicheCount, 0, eliteCount - shapeNicheCount);
        }
        int scoreBandCount = 0;
        if (config_.scoreBandRate > 0.0) {
            scoreBandCount = static_cast<int>(std::lround(config_.scoreBandRate * config_.beamWidth));
            scoreBandCount = std::clamp(scoreBandCount, 0, eliteCount - shapeNicheCount - behaviorNicheCount);
        }
        int nicheCount = shapeNicheCount + behaviorNicheCount + scoreBandCount;
        int pureEliteCount = eliteCount - nicheCount;

        if (nicheCount > 0 && static_cast<int>(scored.size()) > pureEliteCount) {
            // Pure top-score core, exactly as before (move-constructed:
            // CandidateRecord holds a unique_ptr program, not copyable).
            std::vector<CandidateRecord> nextBeam;
            size_t coreEnd = std::min(static_cast<size_t>(pureEliteCount), scored.size());
            nextBeam.reserve(static_cast<size_t>(eliteCount));
            for (size_t i = 0; i < coreEnd; ++i) nextBeam.push_back(std::move(scored[i]));

            // Fill the shape-niche slots with the best NOT-yet-covered AST
            // shape found further down the ranking -- scanning in sorted
            // order means the first candidate of a given shape we see is
            // already that shape's best-scoring representative.
            if (shapeNicheCount > 0) {
                std::unordered_set<std::string> shapesInBeam;
                for (const auto& b : nextBeam) shapesInBeam.insert(structuralShape(*b.program));
                int shapeTarget = pureEliteCount + shapeNicheCount;
                for (size_t i = static_cast<size_t>(pureEliteCount);
                     i < scored.size() && static_cast<int>(nextBeam.size()) < shapeTarget; ++i) {
                    if (!scored[i].program) continue;  // already moved into nextBeam by an earlier pass
                    std::string shape = structuralShape(*scored[i].program);
                    if (shapesInBeam.insert(shape).second) {
                        nextBeam.push_back(std::move(scored[i]));
                    }
                }
            }

            // Fill the behavior-niche slots the same way, but keyed by
            // which instances the candidate got right (behaviorSignature)
            // instead of by AST shape -- see SearchConfig::behaviorNicheRate.
            if (behaviorNicheCount > 0) {
                std::unordered_set<std::string> behaviorsInBeam;
                for (const auto& b : nextBeam) behaviorsInBeam.insert(behaviorSignature(b));
                int behaviorTarget = pureEliteCount + shapeNicheCount + behaviorNicheCount;
                for (size_t i = static_cast<size_t>(pureEliteCount);
                     i < scored.size() && static_cast<int>(nextBeam.size()) < behaviorTarget; ++i) {
                    if (!scored[i].program) continue;  // already claimed by the elite core or shape-niche pass
                    std::string behavior = behaviorSignature(scored[i]);
                    if (behaviorsInBeam.insert(behavior).second) {
                        nextBeam.push_back(std::move(scored[i]));
                    }
                }
            }

            // Fill the score-band slots: one representative per score
            // decile, taken in ranking order so each band contributes its
            // own best member. Because the elite core has already claimed
            // the top bands, this pass necessarily reaches DOWN the
            // ranking and keeps lineages the beam would otherwise discard
            // -- the whole point (see SearchConfig::scoreBandRate).
            if (scoreBandCount > 0) {
                auto bandOf = [&](const CandidateRecord& c) {
                    int denom = std::max(1, c.totalTests);
                    return c.score * kScoreBands / (denom + 1);
                };
                std::unordered_set<int> bandsInBeam;
                for (const auto& b : nextBeam) bandsInBeam.insert(bandOf(b));
                for (size_t i = static_cast<size_t>(pureEliteCount);
                     i < scored.size() && static_cast<int>(nextBeam.size()) < eliteCount; ++i) {
                    if (!scored[i].program) continue;  // already claimed by an earlier pass
                    if (bandsInBeam.insert(bandOf(scored[i])).second) {
                        nextBeam.push_back(std::move(scored[i]));
                    }
                }
            }
            beam = std::move(nextBeam);
        } else {
            if (static_cast<int>(scored.size()) > eliteCount) scored.resize(static_cast<size_t>(eliteCount));
            beam = std::move(scored);
        }

        if (adaptiveBiasOn) {
            // Measured on the fitness-selected elite only (before random
            // immigrants are added below), since those are the survivors
            // whose structure the search is actually converging on.
            int withSkeleton = 0;
            for (const auto& b : beam) {
                if (hasCountingLoopSkeleton(*b.program)) withSkeleton++;
            }
            double skeletonRate = beam.empty() ? 0.0 : static_cast<double>(withSkeleton) / static_cast<double>(beam.size());
            double biasBefore = config_.genConfig.loopAccumulatorBias;
            double rate = config_.genConfig.adaptiveSkeletonBiasRate;
            double biasAfter = biasBefore + rate * (skeletonRate - biasBefore);
            biasAfter = std::clamp(biasAfter, config_.genConfig.adaptiveSkeletonBiasMin, config_.genConfig.adaptiveSkeletonBiasMax);
            config_.genConfig.loopAccumulatorBias = biasAfter;
            if (adaptiveLog.is_open()) {
                adaptiveLog << size << "," << currentLevel << "," << biasBefore << "," << skeletonRate << "," << biasAfter << ","
                            << bestScore << "\n";
            }
        }

        if (immigrantCount > 0) {
            // Random immigrants: fill the rest of the beam with brand-new
            // random programs regardless of their score, instead of the
            // next-best-by-fitness candidates that would otherwise take
            // those slots. Scored the same way as every other candidate
            // (so their timeouts/errors/score still show up in stats and
            // they're eligible to trigger a full solve like anything
            // else) -- the only thing that changed is which candidates get
            // to occupy the tail of the beam.
            std::vector<Program> immigrantPool;
            immigrantPool.reserve(static_cast<size_t>(immigrantCount));
            for (int i = 0; i < immigrantCount; ++i) immigrantPool.push_back(generateRandomProgram(rng, size, config_.genConfig));
            std::vector<CandidateRecord> immigrantScored(immigrantPool.size());
            int immTimeouts = 0, immErrors = 0;
            scoreCandidates(immigrantPool, instances, problem_, interp, threadCount, immigrantScored, 0, immTimeouts, immErrors);
            timeouts += immTimeouts;
            errors += immErrors;
            for (auto& rec : immigrantScored) beam.push_back(std::move(rec));
            // Re-sort so beam.front() (relied on below as "the champion")
            // stays correct: elite is already in score order, but an
            // immigrant could in principle tie or (if eliteCount==0, i.e.
            // diversityRate==1.0) be the actual best of what's left.
            std::sort(beam.begin(), beam.end(), [](const CandidateRecord& a, const CandidateRecord& b) {
                if (a.score != b.score) return a.score > b.score;
                if (a.fitness != b.fitness) return a.fitness > b.fitness;
                return a.size < b.size;
            });
            if (!beam.empty()) bestScore = beam.front().score;
        }

        // Moção 1 (Etapa 2): while the divergence window is open, frozen
        // champions are excluded from the beam so the elite slots grow
        // alternatives instead of re-selecting the same champion every size.
        // (No-op in the default postSolveSizes=0 mode: frozenChampions stays
        // empty and beam.front() remains exactly what the original code used.)
        if (postSolveWindowOpen && !frozenChampions.empty()) {
            beam.erase(std::remove_if(beam.begin(), beam.end(),
                                      [&](const CandidateRecord& c) {
                                          return c.program && frozenChampions.count(canonicalText(*c.program)) > 0;
                                      }),
                       beam.end());
            bestScore = beam.empty() ? 0 : beam.front().score;
        }

        if (bestScore > 0 && bestScore == static_cast<int>(instances.size())) {
            // Full solve of the current level. First, a dry run: how far
            // does the SAME unmutated program cascade forward without ever
            // touching score/selection/mutation? This is the structural
            // invariance check -- a program that only fits this exact
            // level's instance family (a "gambiarra") will fail the very
            // next level and get rejected below; only a program that keeps
            // working across a window of levels gets credited as a real
            // solve. Nothing here changes how candidates are generated.
            const Program& provenProgram = beam.front().program;
            std::string programText = provenProgram->toString();
            int programSize = beam.front().size;
            std::string canonicalProgText = canonicalText(*provenProgram);
            std::string programId = programIdOf(canonicalProgText);

            // Diversity snapshot, purely observational: how many distinct
            // coarse "shapes" survive in the beam right now, at the exact
            // moment a full solve is found? A beam saturated with minor
            // variants of one shape (diversity close to 1) is evidence of
            // premature convergence -- the population narrowed onto a
            // single lineage before it had a chance to discover a more
            // general structure, which may be exactly why min/max/sum keep
            // hitting a wall while contains did not.
            std::unordered_set<std::string> shapes;
            for (const auto& b : beam) shapes.insert(structuralShape(*b.program));
            int beamDiversity = static_cast<int>(shapes.size());
            int beamSizeNow = static_cast<int>(beam.size());
            log::info("  beam diversity at full solve: " + std::to_string(beamDiversity) + " distinct shapes / " +
                      std::to_string(beamSizeNow) + " candidates");

            struct LevelResult {
                int level, score, total;
            };
            std::vector<LevelResult> cascade = {{currentLevel, bestScore, static_cast<int>(instances.size())}};

            int level = currentLevel;
            while (true) {
                int next = problem_.nextLevel(level);
                if (next > config_.maxLevel) break;
                std::vector<Instance> nextInstances = problem_.generateInstances(next);
                int s = 0;
                for (const auto& inst : nextInstances) {
                    if (problem_.evaluate(*provenProgram, inst, interp).status == ExecStatus::SUCCESS) s++;
                }
                if (s != static_cast<int>(nextInstances.size())) break;
                level = next;
                cascade.push_back({level, s, static_cast<int>(nextInstances.size())});
            }

            int firstLevel = currentLevel;
            int confirmedLevels = static_cast<int>(cascade.size());

            int requiredLevel = std::min(currentLevel + config_.invarianceLookahead, config_.maxLevel);
            // A first solve AT maxLevel has no level beyond it to check: the
            // capped requiredLevel equals currentLevel and the check used to
            // pass trivially, labeling any program that memorized the last
            // level's instances SUCCESS (FINDINGS.md section 37). Such a
            // cascade confirmed nothing, so it stays UNCONFIRMED.
            bool lookaheadPossible = currentLevel < config_.maxLevel;
            bool invarianceHolds =
                config_.invarianceLookahead <= 0 || (lookaheadPossible && level >= requiredLevel);

            // Search progression ALWAYS advances on the cascade reached here
            // (exactly like before the gate existed): the engine still needs
            // to score candidates against harder levels to have any chance
            // of finding a program that generalizes to them. Gating this
            // would starve the search -- confirmed by experiment: even
            // ContainsProblem's genuinely-generalizing 92-node program (see
            // data/champion_logs/log_contains.txt) never got discovered when
            // currentLevel was blocked from advancing past a rejected level,
            // because it's only findable while scoring against level 2's
            // instances. What the gate actually changes is which cascades
            // get labeled a confirmed, validated algorithm vs. a provisional
            // fit -- pure bookkeeping on top of unchanged search dynamics.
            std::string status = invarianceHolds ? "SUCCESS" : "UNCONFIRMED";
            if (!invarianceHolds && !lookaheadPossible) {
                log::info("  UNCONFIRMED (no lookahead possible): first solves N=" + std::to_string(currentLevel) +
                          " at the maximum level, so nothing beyond it can confirm the program generalizes");
            } else if (!invarianceHolds) {
                log::info("  UNCONFIRMED (fails structural invariance): solves N=" + std::to_string(currentLevel) +
                          " but breaks before N=" + std::to_string(requiredLevel) +
                          " with the same unmutated program -- likely fit to that N's instances, not a general algorithm");
            }
            for (const auto& lr : cascade) {
                // Dedup guard: with a divergence window the same program can be
                // re-found as champion at a level it already recorded during an
                // earlier cascade; keep only the first record per program@level.
                if (!recordedKeys.insert(programId + "@" + std::to_string(lr.level)).second) continue;
                int advPassed = -1, advTotal = -1;
                std::vector<Instance> adversarial = problem_.generateAdversarialInstances(lr.level);
                if (!adversarial.empty()) {
                    advTotal = static_cast<int>(adversarial.size());
                    advPassed = 0;
                    for (const auto& inst : adversarial) {
                        if (problem_.evaluate(*provenProgram, inst, interp).status == ExecStatus::SUCCESS) advPassed++;
                    }
                }
                if (lr.level != currentLevel) log::advancingToLevel(lr.level);
                log::championAtLevel(lr.level, lr.score, lr.total, programSize, programText, advPassed, advTotal, programId,
                                      firstLevel, confirmedLevels);

                SolutionRecord rec;
                rec.programId = programId;
                rec.programText = programText;
                rec.programSize = programSize;
                rec.levelReached = lr.level;
                rec.nReached = lr.level;
                rec.firstLevel = firstLevel;
                rec.confirmedLevels = confirmedLevels;
                rec.score = lr.score;
                rec.totalTests = lr.total;
                rec.finalStatus = status;
                rec.adversarialPassed = advPassed;
                rec.adversarialTotal = advTotal;
                rec.beamDiversity = beamDiversity;
                rec.beamSize = beamSizeNow;
                rec.seed = config_.seed;
                rec.config = configToPairs(config_);
                solutions.push_back(std::move(rec));
                if (outPrograms) outPrograms->push_back(provenProgram->clone());
            }

            if (config_.postSolveSizes > 0) {
                if (!postSolveWindowOpen) {
                    // First full solve of this level: freeze the champion and
                    // open the divergence window instead of promoting now.
                    postSolveWindowOpen = true;
                    postSolveWindowOpenedThisSize = true;
                    postSolveRemaining = config_.postSolveSizes;
                    postSolveBestCascadeLevel = level;
                    postSolveBestSize = programSize;
                    postSolveBestFitness = beam.front().fitness;
                    postSolvePendingAdvance = problem_.nextLevel(level);
                    frozenChampions.insert(canonicalProgText);
                    log::info("  divergence window open: staying at N=" + std::to_string(currentLevel) + " for " +
                              std::to_string(config_.postSolveSizes) + " more size step(s) after this first full solve");
                } else if (frozenChampions.insert(canonicalProgText).second) {
                    // A genuinely new champion during the window: archive it
                    // (already recorded above) and let it win the promotion if
                    // its cascade reaches farther; on a cascade tie, prefer
                    // higher speed-bonus-weighted fitness when speedBonusWeight
                    // is on (a faster program should be able to beat a smaller-
                    // but-slower one), falling back to the original
                    // smaller-size tie-break when speedBonusWeight is off
                    // (config_.speedBonusWeight == 0.0 reproduces prior
                    // behavior exactly).
                    bool better;
                    if (level != postSolveBestCascadeLevel) {
                        better = level > postSolveBestCascadeLevel;
                    } else if (config_.speedBonusWeight > 0.0) {
                        better = beam.front().fitness > postSolveBestFitness;
                    } else {
                        better = programSize < postSolveBestSize;
                    }
                    if (better) {
                        postSolveBestCascadeLevel = level;
                        postSolveBestSize = programSize;
                        postSolveBestFitness = beam.front().fitness;
                        postSolvePendingAdvance = problem_.nextLevel(level);
                    }
                    log::info("  divergence window: archived an alternative champion at N=" +
                              std::to_string(currentLevel));
                }
            } else {
                currentLevel = problem_.nextLevel(level);
                if (!config_.checkpointPath.empty()) saveSolutions(config_.checkpointPath, solutions);
            }
        }

        // Moção 1 (Etapa 2): close the divergence window once the extra size
        // budget elapses, then promote to the level the best archived
        // champion's cascade reached.
        if (postSolveWindowOpen && !postSolveWindowOpenedThisSize) {
            postSolveRemaining--;
            if (postSolveRemaining <= 0) {
                currentLevel = postSolvePendingAdvance;
                postSolveWindowOpen = false;
                postSolveRemaining = 0;
                postSolvePendingAdvance = 0;
                postSolveBestCascadeLevel = 0;
                postSolveBestSize = 0;
                frozenChampions.clear();
                log::info("  divergence window closed: promoting to N=" + std::to_string(currentLevel));
                if (!config_.checkpointPath.empty()) saveSolutions(config_.checkpointPath, solutions);
            }
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - searchStart).count();
    log::searchFinished(elapsed);
    if (elapsed > 0.0) {
        double genRate = totalCandidatesGenerated / elapsed;
        double scoreRate = totalCandidatesScored / elapsed;
        if (!behaviorTallies.empty()) {
            static const char* const originNames[3] = {"mutant", "fresh", "crossover"};
            for (const auto& [level, perOrigin] : behaviorTallies) {
                for (int o = 0; o < 3; ++o) {
                    const BehaviorTally& t = perOrigin[static_cast<size_t>(o)];
                    if (t.candidates == 0) continue;
                    std::cerr << "[behavior-debug] level=" << level << " origin=" << originNames[o]
                              << " candidates=" << t.candidates << " dup_full=" << t.dupFull
                              << " dup_full_zero_score=" << t.dupFullZeroScore << " dup_prefix=" << t.dupPrefix
                              << " prefix_false_merge=" << t.prefixFalseMerge << " neutral=" << t.neutral << " new_clean=" << t.newClean << "\n";
                }
            }
        }
        std::cout << "  profile: gen=" << genPhaseSeconds << "s score=" << scorePhaseSeconds << "s sort=" << sortPhaseSeconds
                  << "s other=" << (elapsed - genPhaseSeconds - scorePhaseSeconds - sortPhaseSeconds) << "s | generated "
                  << totalCandidatesGenerated << " (" << static_cast<long>(genRate) << "/s), scored " << totalCandidatesScored
                  << " (" << static_cast<long>(scoreRate) << "/s)\n";
    }
    return solutions;
}

}  // namespace algosearch
