#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ast/Stmt.hpp"
#include "problems/Problem.hpp"

namespace algosearch {

// One (input size, average wall-clock time in nanoseconds per call) point.
struct TimingPoint {
    int n;
    double avgNanos;
};

struct BenchmarkResult {
    std::string programId;
    std::string programText;
    int programSize = 0;
    // Empirical re-verification, independent of whatever label the search
    // itself attached (SUCCESS/UNCONFIRMED): does this exact program solve
    // freshly generated instances correctly across a wide, unrelated range
    // of sizes? Only results with generalizes==true are meaningful to rank
    // or benchmark -- see verifyGeneralization().
    bool generalizes = false;
    std::string failureReason;  // set when generalizes == false
    std::vector<TimingPoint> timings;
    // Empirical complexity: time ~ c * n^exponent, fit by ordinary least
    // squares on (log n, log time). r2 close to 1 means the power-law fit
    // is a good description of the measured growth; a poor r2 (small
    // range, noisy timings, or genuinely non-power-law growth like O(log n)
    // or a plateau) means the exponent alone shouldn't be over-trusted.
    double exponent = 0.0;
    double r2 = 0.0;
    std::string complexityLabel;  // nearest human label for `exponent`
};

// Independently re-checks that `program` is a correct solution across a
// wide range of instance sizes (using Problem::generateBenchmarkInstance,
// several random seeds per size) -- NOT just the training/held-out split
// the search already scored it against. This is the actual generalization
// gate for the benchmark tool: a program that only fits the specific N's
// it was searched against (a "gambiarra") will fail here even if the
// search itself labeled it SUCCESS/UNCONFIRMED. Returns the failure
// description in `outReason` when it returns false.
// seedRecursionBounds: pass through whatever the search itself used (see
// Interpreter's constructor) so a program that relies on the recursion
// extension's v1(lo)/v2(hi) convention is re-verified under the exact same
// semantics it was found under, not silently different ones.
bool verifyGeneralization(const Problem& problem, const Stmt& program, long stepLimit, std::string& outReason,
                          bool seedRecursionBounds = false);

// Times `program` at each size in `sizes`, running enough repetitions per
// size to get a stable average (see Benchmark.cpp for the calibration
// rule). Assumes the program has already passed verifyGeneralization.
std::vector<TimingPoint> timeProgram(const Problem& problem, const Stmt& program, long stepLimit, const std::vector<int>& sizes,
                                      bool seedRecursionBounds = false);

// Fits time ~ c * n^exponent via least squares on log-log data and
// classifies the exponent into a human-readable complexity label.
// Returns {exponent, r2}.
std::pair<double, double> fitPowerLaw(const std::vector<TimingPoint>& timings);

std::string classifyExponent(double exponent);

}  // namespace algosearch
