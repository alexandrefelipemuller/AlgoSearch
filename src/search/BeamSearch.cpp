#include "search/BeamSearch.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "search/Dedup.hpp"
#include "search/Logger.hpp"

namespace algosearch {

namespace {

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

        if (static_cast<int>(scored.size()) > config_.beamWidth) scored.resize(static_cast<size_t>(config_.beamWidth));
        beam = std::move(scored);

        if (bestScore > 0 && bestScore == static_cast<int>(instances.size())) {
            // Full solve of the current level: promote as far as possible.
            const Program& provenProgram = beam.front().program;
            int level = currentLevel;
            int reachedScore = bestScore;
            int reachedTotal = static_cast<int>(instances.size());

            while (true) {
                int next = problem_.nextLevel(level);
                if (next > config_.maxLevel) break;
                std::vector<Instance> nextInstances = problem_.generateInstances(next);
                int s = 0;
                for (const auto& inst : nextInstances) {
                    if (problem_.evaluate(*provenProgram, inst, interp).status == ExecStatus::SUCCESS) s++;
                }
                if (s == static_cast<int>(nextInstances.size())) {
                    level = next;
                    reachedScore = s;
                    reachedTotal = static_cast<int>(nextInstances.size());
                    log::advancingToLevel(level);
                } else {
                    break;
                }
            }

            log::solutionFound(beam.front().size, provenProgram->toString());

            // Diagnostic only: check the proven program against
            // out-of-distribution instances (duplicates, negatives, wide
            // range, sorted, reversed) that generateInstances() never
            // produces. This never affects score/promotion -- it exists so
            // "SOLUTION FOUND" doesn't get mistaken for "this generalizes to
            // any array", which it may well not.
            int advPassed = -1, advTotal = -1;
            std::vector<Instance> adversarial = problem_.generateAdversarialInstances(level);
            if (!adversarial.empty()) {
                advTotal = static_cast<int>(adversarial.size());
                advPassed = 0;
                for (const auto& inst : adversarial) {
                    if (problem_.evaluate(*provenProgram, inst, interp).status == ExecStatus::SUCCESS) advPassed++;
                }
                log::info("  generalization check (out-of-distribution) = " + std::to_string(advPassed) + "/" +
                          std::to_string(advTotal));
            }

            SolutionRecord rec;
            rec.programText = provenProgram->toString();
            rec.programSize = beam.front().size;
            rec.levelReached = level;
            rec.nReached = level;
            rec.score = reachedScore;
            rec.totalTests = reachedTotal;
            rec.finalStatus = "SUCCESS";
            rec.adversarialPassed = advPassed;
            rec.adversarialTotal = advTotal;
            rec.seed = config_.seed;
            rec.config = configToPairs(config_);
            solutions.push_back(std::move(rec));

            currentLevel = problem_.nextLevel(level);
        }
    }

    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - searchStart).count();
    log::searchFinished(elapsed);
    return solutions;
}

}  // namespace algosearch
