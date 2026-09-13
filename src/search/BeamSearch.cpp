#include "search/BeamSearch.hpp"

#include <algorithm>
#include <chrono>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"
#include "search/Dedup.hpp"
#include "search/Logger.hpp"

namespace algosearch {

namespace {

struct CandidateRecord {
    Program program;
    int size = 0;
    int score = 0;
    int totalTests = 0;
};

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
        std::vector<CandidateRecord> scored;
        scored.reserve(grownPool.size() + freshPool.size());
        auto scoreAndCollect = [&](std::vector<Program>& pool) {
            for (auto& prog : pool) {
                int score = 0;
                for (const auto& inst : instances) {
                    EvalOutcome outcome = problem_.evaluate(*prog, inst, interp);
                    if (outcome.status == ExecStatus::SUCCESS) {
                        score++;
                    } else if (outcome.status == ExecStatus::TIMEOUT) {
                        timeouts++;
                    } else if (outcome.status == ExecStatus::RUNTIME_ERROR) {
                        errors++;
                    }
                }
                CandidateRecord rec;
                rec.size = prog->size();
                rec.score = score;
                rec.totalTests = static_cast<int>(instances.size());
                rec.program = std::move(prog);
                scored.push_back(std::move(rec));
            }
        };
        scoreAndCollect(grownPool);
        scoreAndCollect(freshPool);

        if (scored.empty()) {
            log::progress(0, 0, static_cast<int>(instances.size()), 0, timeouts, errors);
            continue;
        }

        std::sort(scored.begin(), scored.end(), [](const CandidateRecord& a, const CandidateRecord& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.size < b.size;  // among equal scores, prefer smaller programs to keep the beam diverse/simple
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

            SolutionRecord rec;
            rec.programText = provenProgram->toString();
            rec.programSize = beam.front().size;
            rec.levelReached = level;
            rec.nReached = level;
            rec.score = reachedScore;
            rec.totalTests = reachedTotal;
            rec.finalStatus = "SUCCESS";
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
