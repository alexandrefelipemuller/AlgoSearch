#include "search/BeamSearch.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <thread>
#include <unordered_set>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "search/Dedup.hpp"
#include "search/Logger.hpp"

namespace algosearch {

namespace {

// Stable, short id for a program: a hex hash of its canonical text. Every
// per-N row produced by one promotion cascade shares this id, since they
// all describe the exact same unmutated program.
std::string programIdOf(const std::string& programText) {
    std::ostringstream oss;
    oss << std::hex << std::hash<std::string>{}(programText);
    return oss.str();
}

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
        case StmtKind::Block: break;
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
    double fitness = 0;  // score + inversions-based partial credit; search-internal
                          // ranking heuristic only, never substitutes for `score`
    int totalTests = 0;
};

// Scores every candidate in `pool` against `instances`, writing results into
// `scored` starting at `outOffset` (one CandidateRecord per candidate, same
// order). Splits the work across threads: each candidate is evaluated fully
// independently (Interpreter::run and Problem methods touch no shared
// mutable state), so this is embarrassingly parallel and needs no locking.
void scoreCandidates(std::vector<Program>& pool, const std::vector<Instance>& instances, const Problem& problem,
                      const Interpreter& interp, unsigned numThreads, std::vector<CandidateRecord>& scored, size_t outOffset,
                      int& timeouts, int& errors) {
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
            for (size_t i = begin; i < end; ++i) {
                CandidateRecord& rec = scored[outOffset + i];
                int score = 0;
                double fitness = 0.0;
                for (const auto& inst : instances) {
                    EvalOutcome outcome = problem.evaluate(*rec.program, inst, interp);
                    if (outcome.status == ExecStatus::SUCCESS) {
                        score++;
                        fitness += 1.0;
                    } else if (outcome.status == ExecStatus::TIMEOUT) {
                        timeoutsPerThread[t]++;
                    } else if (outcome.status == ExecStatus::RUNTIME_ERROR) {
                        errorsPerThread[t]++;
                    } else {
                        fitness += problem.partialScore(inst.input, outcome.output);
                    }
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
        {"loop_bias", std::to_string(cfg.genConfig.loopAccumulatorBias)},
        {"adaptive_skeleton_bias_rate", std::to_string(cfg.genConfig.adaptiveSkeletonBiasRate)},
    };
}

}  // namespace

BeamSearch::BeamSearch(const Problem& problem, SearchConfig config) : problem_(problem), config_(std::move(config)) {}

std::vector<SolutionRecord> BeamSearch::run() {
    Random rng(config_.seed);
    Interpreter interp(config_.stepLimit);
    Dedup dedup;

    std::vector<CandidateRecord> beam;
    int currentLevel = config_.startLevel;
    std::vector<SolutionRecord> solutions;

    auto searchStart = std::chrono::steady_clock::now();

    std::ofstream adaptiveLog;
    bool adaptiveBiasOn = config_.genConfig.adaptiveSkeletonBiasRate > 0.0;
    if (adaptiveBiasOn && !config_.adaptiveBiasLogPath.empty()) {
        adaptiveLog.open(config_.adaptiveBiasLogPath);
        adaptiveLog << "round,level,bias_before,skeleton_rate,bias_after,best_score\n";
    }

    for (int size = 1; size <= config_.maxProgramSize && currentLevel <= config_.maxLevel; ++size) {
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
        std::vector<Program> grownPool;
        grownPool.reserve(beam.size() * 2);
        for (auto& b : beam) {
            grownPool.push_back(b.program->clone());
            grownPool.push_back(growProgram(*b.program, size, rng, config_.genConfig));
        }

        std::vector<Program> freshPool;
        int guard = 0;
        while (static_cast<int>(freshPool.size()) < config_.candidatesPerSize && guard < config_.candidatesPerSize * 4) {
            auto candidate = generateRandomProgram(rng, size, config_.genConfig);
            if (dedup.insertIfNew(*candidate)) freshPool.push_back(std::move(candidate));
            guard++;
        }

        int timeouts = 0, errors = 0;
        unsigned threadCount = config_.numThreads != 0 ? config_.numThreads : std::thread::hardware_concurrency();
        if (threadCount == 0) threadCount = 1;

        std::vector<CandidateRecord> scored(grownPool.size() + freshPool.size());
        scoreCandidates(grownPool, instances, problem_, interp, threadCount, scored, 0, timeouts, errors);
        scoreCandidates(freshPool, instances, problem_, interp, threadCount, scored, grownPool.size(), timeouts, errors);

        if (scored.empty()) {
            log::progress(0, 0, static_cast<int>(instances.size()), 0, timeouts, errors);
            continue;
        }

        // Rank primarily by exact solved-count (the only metric that ever
        // decides promotion), then by inversions-based partial fitness as a
        // gradient when counts tie (crucial when every candidate solves 0
        // instances and a binary score gives the beam nothing to climb),
        // then prefer smaller programs to keep the beam diverse/simple.
        std::sort(scored.begin(), scored.end(), [](const CandidateRecord& a, const CandidateRecord& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.fitness != b.fitness) return a.fitness > b.fitness;
            return a.size < b.size;
        });

        int bestScore = scored.front().score;
        int bestSize = scored.front().size;
        log::progress(scored.size(), bestScore, static_cast<int>(instances.size()), bestSize, timeouts, errors);

        int immigrantCount = 0;
        int eliteCount = config_.beamWidth;
        if (config_.diversityRate > 0.0) {
            immigrantCount = static_cast<int>(std::lround(config_.diversityRate * config_.beamWidth));
            immigrantCount = std::clamp(immigrantCount, 0, config_.beamWidth);
            eliteCount = config_.beamWidth - immigrantCount;
        }
        if (static_cast<int>(scored.size()) > eliteCount) scored.resize(static_cast<size_t>(eliteCount));
        beam = std::move(scored);

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
            std::string programId = programIdOf(programText);

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
            bool invarianceHolds = config_.invarianceLookahead <= 0 || level >= requiredLevel;

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
            if (!invarianceHolds) {
                log::info("  UNCONFIRMED (fails structural invariance): solves N=" + std::to_string(currentLevel) +
                          " but breaks before N=" + std::to_string(requiredLevel) +
                          " with the same unmutated program -- likely fit to that N's instances, not a general algorithm");
            }
            for (const auto& lr : cascade) {
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
            }
            currentLevel = problem_.nextLevel(level);
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - searchStart).count();
    log::searchFinished(elapsed);
    return solutions;
}

}  // namespace algosearch
