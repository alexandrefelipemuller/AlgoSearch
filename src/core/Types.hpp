#pragma once

namespace algosearch {

// Final classification of running a discovered program against one problem instance.
enum class ExecStatus {
    SUCCESS,       // ran to completion and produced the expected output
    WRONG_OUTPUT,  // ran to completion but output did not match expectation
    TIMEOUT,       // exceeded the deterministic VM step limit
    RUNTIME_ERROR  // e.g. out-of-bounds array access
};

inline const char* toString(ExecStatus s) {
    switch (s) {
        case ExecStatus::SUCCESS: return "SUCCESS";
        case ExecStatus::WRONG_OUTPUT: return "WRONG_OUTPUT";
        case ExecStatus::TIMEOUT: return "TIMEOUT";
        case ExecStatus::RUNTIME_ERROR: return "RUNTIME_ERROR";
    }
    return "UNKNOWN";
}

// Raw outcome of interpreting a program, before comparing against expected output.
enum class RunStatus { COMPLETED, TIMEOUT, RUNTIME_ERROR };

}  // namespace algosearch
