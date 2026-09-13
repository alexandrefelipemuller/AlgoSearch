#pragma once

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

    // Whether `output` is a correct solution for `input`.
    virtual bool isSolution(const std::vector<int>& input, const std::vector<int>& output) const = 0;

    // Number of scalar integer variables a generated program may use.
    virtual int numVars() const = 0;

    // Runs `program` against `instance` using `interp`, then classifies the
    // result via isSolution(). Problem-agnostic; implemented once here.
    EvalOutcome evaluate(const Stmt& program, const Instance& instance, const Interpreter& interp) const {
        std::vector<int> output;
        RunStatus rs = interp.run(program, instance.input, numVars(), output);
        switch (rs) {
            case RunStatus::TIMEOUT:
                return {ExecStatus::TIMEOUT, {}};
            case RunStatus::RUNTIME_ERROR:
                return {ExecStatus::RUNTIME_ERROR, {}};
            case RunStatus::COMPLETED:
                if (isSolution(instance.input, output)) {
                    return {ExecStatus::SUCCESS, output};
                }
                return {ExecStatus::WRONG_OUTPUT, output};
        }
        return {ExecStatus::RUNTIME_ERROR, {}};
    }
};

}  // namespace algosearch
