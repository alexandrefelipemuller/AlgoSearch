#pragma once

#include <unordered_set>

#include "ast/Stmt.hpp"

namespace algosearch {

// Deduplicates structurally identical ASTs (not semantically equivalent
// ones) by canonical source-text serialization. Cheap and sufficient for
// avoiding wasted re-exploration of the exact same program.
class Dedup {
public:
    // Returns true if `program` was not seen before (and records it).
    bool insertIfNew(const Stmt& program) {
        std::string key = program.toString();
        return seen_.insert(std::move(key)).second;
    }

    bool contains(const Stmt& program) const { return seen_.count(program.toString()) > 0; }

    size_t size() const { return seen_.size(); }

private:
    std::unordered_set<std::string> seen_;
};

}  // namespace algosearch
