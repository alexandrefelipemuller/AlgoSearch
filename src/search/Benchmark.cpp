#include "search/Benchmark.hpp"

#include <chrono>
#include <cmath>

#include "core/Random.hpp"
#include "interpreter/Interpreter.hpp"

namespace algosearch {

namespace {
// Sizes used to probe generalization, deliberately unrelated to whatever
// levels the search happened to train/hold-out against, and deliberately
// including sizes well beyond any level the search ever saw.
const std::vector<int> kVerificationSizes = {1, 2, 3, 4, 5, 7, 10, 15, 20, 30, 50, 80};
const int kTrialsPerSize = 4;
}  // namespace

bool verifyGeneralization(const Problem& problem, const Stmt& program, long stepLimit, std::string& outReason) {
    Interpreter interp(stepLimit);
    for (int n : kVerificationSizes) {
        for (int t = 0; t < kTrialsPerSize; ++t) {
            uint64_t seed = static_cast<uint64_t>(n) * 7919ULL + static_cast<uint64_t>(t) * 104729ULL + 17;
            Instance inst = problem.generateBenchmarkInstance(n, seed);
            if (inst.input.empty()) continue;  // problem has nothing to test at this n
            EvalOutcome outcome = problem.evaluate(program, inst, interp);
            if (outcome.status != ExecStatus::SUCCESS) {
                outReason = "failed at n=" + std::to_string(n) + " trial=" + std::to_string(t) +
                            (outcome.status == ExecStatus::TIMEOUT      ? " (timeout)"
                             : outcome.status == ExecStatus::WRONG_OUTPUT ? " (wrong output)"
                                                                           : " (runtime error)");
                return false;
            }
        }
    }
    return true;
}

std::vector<TimingPoint> timeProgram(const Problem& problem, const Stmt& program, long stepLimit, const std::vector<int>& sizes) {
    Interpreter interp(stepLimit);
    std::vector<TimingPoint> out;
    for (int n : sizes) {
        Instance inst = problem.generateBenchmarkInstance(n, /*seed=*/12345);
        if (inst.input.empty()) continue;

        // Calibrate repetitions: run a short warm-up batch, then scale up
        // to target at least ~20ms of total wall-clock so timing noise
        // (scheduler jitter, cache effects) averages out, capped so a very
        // slow candidate can't stall the whole benchmark.
        int reps = 20;
        auto runBatch = [&](int count) {
            std::vector<int> output;
            auto start = std::chrono::steady_clock::now();
            for (int i = 0; i < count; ++i) {
                interp.run(program, inst.input, problem.numVars(), output);
            }
            return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
        };

        double warmupNanos = runBatch(reps);
        double targetNanos = 20'000'000.0;  // 20ms
        if (warmupNanos > 0 && warmupNanos < targetNanos) {
            double scale = targetNanos / warmupNanos;
            reps = std::min(200000, std::max(reps, static_cast<int>(reps * scale)));
        }
        double totalNanos = runBatch(reps);
        out.push_back({n, totalNanos / static_cast<double>(reps)});
    }
    return out;
}

std::pair<double, double> fitPowerLaw(const std::vector<TimingPoint>& timings) {
    std::vector<double> xs, ys;
    for (const auto& p : timings) {
        if (p.n <= 0 || p.avgNanos <= 0) continue;
        xs.push_back(std::log(static_cast<double>(p.n)));
        ys.push_back(std::log(p.avgNanos));
    }
    if (xs.size() < 2) return {0.0, 0.0};

    double n = static_cast<double>(xs.size());
    double sumX = 0, sumY = 0, sumXY = 0, sumXX = 0;
    for (size_t i = 0; i < xs.size(); ++i) {
        sumX += xs[i];
        sumY += ys[i];
        sumXY += xs[i] * ys[i];
        sumXX += xs[i] * xs[i];
    }
    double denom = n * sumXX - sumX * sumX;
    if (std::abs(denom) < 1e-12) return {0.0, 0.0};
    double slope = (n * sumXY - sumX * sumY) / denom;
    double intercept = (sumY - slope * sumX) / n;

    double meanY = sumY / n;
    double ssTot = 0, ssRes = 0;
    for (size_t i = 0; i < xs.size(); ++i) {
        double predicted = intercept + slope * xs[i];
        ssRes += (ys[i] - predicted) * (ys[i] - predicted);
        ssTot += (ys[i] - meanY) * (ys[i] - meanY);
    }
    double r2 = ssTot > 1e-12 ? 1.0 - ssRes / ssTot : 1.0;
    return {slope, r2};
}

std::string classifyExponent(double exponent) {
    // Reference exponents for the common classes; pick whichever is
    // nearest the measured slope. O(log n) and O(n log n) don't fit this
    // pure-power-law model exactly, so they're approximated by their
    // dominant polynomial behavior (near-flat / near-linear) -- the r2
    // reported alongside this label is what flags when that approximation
    // is a poor fit and the label shouldn't be trusted at face value.
    struct Ref {
        double exponent;
        const char* label;
    };
    static const Ref refs[] = {
        {0.0, "O(1) -- constant"},
        {0.5, "O(sqrt n) -- square root"},
        {1.0, "O(n) -- linear"},
        {1.5, "O(n^1.5)"},
        {2.0, "O(n^2) -- quadratic"},
        {3.0, "O(n^3) -- cubic"},
    };
    const Ref* best = &refs[0];
    double bestDist = std::abs(exponent - refs[0].exponent);
    for (const auto& r : refs) {
        double d = std::abs(exponent - r.exponent);
        if (d < bestDist) {
            bestDist = d;
            best = &r;
        }
    }
    return best->label;
}

}  // namespace algosearch
