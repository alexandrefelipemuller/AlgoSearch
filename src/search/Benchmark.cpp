#include "search/Benchmark.hpp"

#include <algorithm>
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
const int kTrialsPerSize = 6;
// Magnitude offsets applied to a copy of every verification instance (see
// the shift-stress check below) -- picked far from any small constant the
// grammar can construct directly (maxConst defaults to 3) so a formula
// that happens to land near a plausible array-length value only for the
// bounded/small value ranges the search's own training data uses (e.g.
// generateBaseArrays' [0, 4n) range) gets exposed instead of coincidentally
// passing. Both POSITIVE: a negative offset was tried first and produced a
// false rejection for GcdProblem (its documented domain is positive
// integers only -- shifting an operand below 1 makes the instance
// meaningless for that problem, not a real generalization failure). Two
// positive magnitudes keep every existing problem's stated domain intact
// while still being large enough to break small-number coincidences.
// Originally {5003, 9973} (see git history), but FactorialProblem's
// genuinely-generalizing algorithm is O(n^2) in total interpreter steps
// (a nested multiply-by-repeated-addition loop), and shifting n=1 up to
// ~9973 pushed step counts past this file's benchmark step limit --
// timing out and getting misclassified as a rejected/non-generalizing
// program, not a real generalization failure. Lowered to keep comfortably
// under the step budget even for O(n^2) problems while staying >>10x the
// largest n verifyGeneralization tests (80), still enough margin to catch
// the kind of small-number coincidence this check exists for.
const long kShiftOffsets[] = {997, 1997};
// The shift check only runs at these (smaller) sizes, a subset of
// kVerificationSizes -- for a magnitude-based problem (IsPrimeProblem,
// GcdProblem, FibonacciProblem, FactorialProblem), "n" IS the tested
// value, so shifting it doesn't just perturb values, it inflates the
// actual computational size. That's harmless for an O(n) algorithm but a
// real cost problem for FactorialProblem's genuinely-generalizing O(n^2)
// one: shifting n=80 up by ~2000 means executing factorial(~2080), ~50x
// the work of the unshifted check. The check's purpose (catching a
// formula that only coincidentally works across the small, bounded
// value range training data uses) doesn't need large n to do its job, so
// it's restricted here instead of raising the step budget to
// accommodate worst-case superlinear growth at every size.
const std::vector<int> kShiftStressSizes = {1, 2, 3, 5, 10};

bool checkOneInstance(const Problem& problem, const Stmt& program, const Interpreter& interp, const Instance& inst,
                       const std::string& label, std::string& outReason) {
    EvalOutcome outcome = problem.evaluate(program, inst, interp);
    if (outcome.status != ExecStatus::SUCCESS) {
        outReason = "failed" + label +
                    (outcome.status == ExecStatus::TIMEOUT      ? " (timeout)"
                     : outcome.status == ExecStatus::WRONG_OUTPUT ? " (wrong output)"
                                                                   : " (runtime error)");
        return false;
    }
    return true;
}
}  // namespace

bool verifyGeneralization(const Problem& problem, const Stmt& program, long stepLimit, std::string& outReason,
                           bool seedRecursionBounds) {
    Interpreter interp(stepLimit, /*maxRecursionDepth=*/200, seedRecursionBounds);
    for (int n : kVerificationSizes) {
        for (int t = 0; t < kTrialsPerSize; ++t) {
            uint64_t seed = static_cast<uint64_t>(n) * 7919ULL + static_cast<uint64_t>(t) * 104729ULL + 17;
            Instance inst = problem.generateBenchmarkInstance(n, seed);
            if (inst.input.empty()) continue;  // problem has nothing to test at this n

            std::string label = " at n=" + std::to_string(n) + " trial=" + std::to_string(t);
            if (!checkOneInstance(problem, program, interp, inst, label, outReason)) return false;

            // Magnitude-shift stress check: adds a large constant to every
            // input value (recomputing correctness fresh via
            // problem.evaluate -- never assumes what the "right" answer
            // is, so this is valid for any problem, not special-cased).
            // Loop bounds/counters in a genuine algorithm are separate
            // scalars that start at 0 and never derive from input values,
            // so a real solution is completely unaffected by this; it
            // exists to catch a formula that only coincidentally computes
            // something plausible-looking from the input's magnitude
            // (e.g. reassigning the array-length convention variable via
            // an expression like `arr[1] - target`, which happens to land
            // near the true length only because training data keeps
            // values in a bounded, length-correlated range) -- caught
            // exactly this shape in ContainsProblem's benchmark output
            // before this check existed.
            bool doShiftCheck = problem.supportsMagnitudeShift() &&
                                std::find(kShiftStressSizes.begin(), kShiftStressSizes.end(), n) != kShiftStressSizes.end();
            if (doShiftCheck) {
                for (long offset : kShiftOffsets) {
                    Instance shifted = inst;
                    for (int& v : shifted.input) v = static_cast<int>(static_cast<long>(v) + offset);
                    std::string shiftLabel = label + " [shifted by " + std::to_string(offset) + "]";
                    if (!checkOneInstance(problem, program, interp, shifted, shiftLabel, outReason)) return false;
                }
            }
        }
    }
    return true;
}

std::vector<TimingPoint> timeProgram(const Problem& problem, const Stmt& program, long stepLimit, const std::vector<int>& sizes,
                                      bool seedRecursionBounds) {
    Interpreter interp(stepLimit, /*maxRecursionDepth=*/200, seedRecursionBounds);
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
