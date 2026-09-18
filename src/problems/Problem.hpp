#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ast/Stmt.hpp"
#include "core/Types.hpp"
#include "interpreter/Interpreter.hpp"

namespace algosearch {

// One test case: a structured input array and the expected structured output.
struct Instance {
    std::vector<int> input;
    std::vector<int> expectedOutput;
};

// Result of running one program against one instance.
struct EvalOutcome {
    ExecStatus status;
    std::vector<int> output;
};

// Problem-agnostic interface the search engine (BeamSearch) programs against.
// The engine never knows about sorting specifically -- only about this
// interface -- so new problems can be added without touching search code.
class Problem {
public:
    virtual ~Problem() = default;

    // Generates the test instances for a given difficulty level. Levels start
    // at 1 and increase via nextLevel(). Early levels should be exhaustive
    // whenever the instance space is small enough.
    virtual std::vector<Instance> generateInstances(int level) const = 0;

    // Human readable description of a level, e.g. "N = 3, instances = 6".
    virtual std::string describeLevel(int level) const = 0;

    // The next (harder) level after `level`.
    virtual int nextLevel(int level) const = 0;

    // Extra, deliberately out-of-distribution instances for a level (e.g.
    // duplicates, negative values, wide ranges) that generateInstances()
    // never exercises. Purely diagnostic: never used for scoring or
    // promotion, only to honestly report whether a "solution" generalizes
    // beyond the exact instance family it was searched against, per the
    // project's stance that passing a level's tests is not proof of
    // correctness for all inputs. Default: none.
    virtual std::vector<Instance> generateAdversarialInstances(int /*level*/) const { return {}; }

    // Whether Benchmark::verifyGeneralization's magnitude-shift stress
    // check (adding a large constant to every array value, to catch a
    // formula that only coincidentally works across the small bounded
    // value range training data uses) is a MEANINGFUL perturbation for this
    // problem's instance encoding. True for every ordinary magnitude/array
    // problem (the existing default, unchanged for all of them) -- shifting
    // arbitrary numeric values by a constant keeps the instance in-domain.
    // False for a problem whose array encodes non-shiftable METADATA in
    // specific slots (a declared count that must match the array's actual
    // length, or boolean 0/1 flags whose meaning is destroyed by adding
    // 997 to them) -- see GraphReachabilityProblem, whose vertex-count
    // and adjacency-matrix-flag slots are exactly this shape. This is the
    // same domain-appropriateness principle GcdProblem's shift-offset
    // comment already established (only POSITIVE offsets, to keep gcd's
    // positive-integers-only domain intact) -- generalized into a per-
    // problem opt-out for encodings a uniform shift can't respect at all,
    // rather than trying to pick offsets that happen to work for every
    // possible encoding.
    virtual bool supportsMagnitudeShift() const { return true; }

    // Whether `output` is a correct solution for `input`.
    virtual bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const = 0;

    // Continuous credit in [0, 1] for a WRONG_OUTPUT result: how close
    // `output` is to a correct solution for `input`, 1.0 meaning correct.
    // This is a search heuristic ONLY -- it never substitutes for
    // isSolution() when deciding correctness/promotion, it exists purely to
    // give beam search a gradient to climb when every candidate's exact
    // solved-count is flat (e.g. 0/40), which a binary score cannot
    // provide. Default: no partial credit (binary 0/1 via isSolution).
    virtual double partialScore(const std::vector<int>& input, const std::vector<int>& output) const {
        return isSolution(input, output) ? 1.0 : 0.0;
    }

    // Number of scalar integer variables a generated program may use.
    virtual int numVars() const = 0;

    // Builds one instance whose "size" is `n`, for empirical runtime
    // benchmarking -- distinct from generateInstances(level) because for
    // array problems "n" and "level" coincide (array length), but for
    // scalar-magnitude problems (is_prime, gcd) level maps to a *range* via
    // an internal formula, not a directly usable size. Default: reuses
    // generateInstances(n) and picks one element deterministically from
    // `seed` -- correct for every array problem as-is. Scalar problems
    // override this so `n` means what a caller benchmarking runtime growth
    // actually wants it to mean (e.g. is_prime: the value tested; gcd: the
    // larger operand).
    virtual Instance generateBenchmarkInstance(int n, uint64_t seed) const {
        std::vector<Instance> pool = generateInstances(n);
        if (pool.empty()) return {{}, {}};
        return pool[seed % pool.size()];
    }

    // Runs `program` against `instance` using `interp`, then classifies the
    // result via isSolution(). Problem-agnostic; implemented once here.
    // `output` is a caller-owned scratch buffer so the hot scoring loop can
    // reuse one allocation across millions of runs; it holds the program's
    // final array only when the result is SUCCESS/WRONG_OUTPUT. `stepsOut`,
    // when non-null, receives the interpreter step count the run consumed
    // -- see Interpreter::run; optional, default null, no cost for callers
    // that don't need a speed signal.
    ExecStatus evaluateInto(const Stmt& program, const Instance& instance, const Interpreter& interp,
                            std::vector<int>& output, long* stepsOut = nullptr) const {
        RunStatus rs = interp.run(program, instance.input, numVars(), output, stepsOut);
        switch (rs) {
            case RunStatus::TIMEOUT:
                return ExecStatus::TIMEOUT;
            case RunStatus::RUNTIME_ERROR:
                return ExecStatus::RUNTIME_ERROR;
            case RunStatus::COMPLETED:
                return isSolution(instance.input, output) ? ExecStatus::SUCCESS : ExecStatus::WRONG_OUTPUT;
        }
        return ExecStatus::RUNTIME_ERROR;
    }

    // Convenience form for non-hot callers (tests, verification): same
    // classification, but allocates its own output vector.
    EvalOutcome evaluate(const Stmt& program, const Instance& instance, const Interpreter& interp) const {
        std::vector<int> output;
        ExecStatus st = evaluateInto(program, instance, interp, output);
        if (st == ExecStatus::TIMEOUT || st == ExecStatus::RUNTIME_ERROR) return {st, {}};
        return {st, std::move(output)};
    }
};

}  // namespace algosearch
