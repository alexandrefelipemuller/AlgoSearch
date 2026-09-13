#pragma once

#include <cstddef>
#include <functional>
#include <unordered_set>

#include "ast/Stmt.hpp"

namespace algosearch {

// Deduplicates structurally identical ASTs (not semantically equivalent
// ones) by canonical source-text serialization. Cheap and sufficient for
// avoiding wasted re-exploration of the exact same program.
//
// Stores a hash of the canonical text rather than the text itself: with
// large programs (deep beam-search runs can reach thousands of AST nodes)
// and many thousands of candidates per size step, keeping full source
// strings around for the whole search is unbounded and can exhaust system
// memory (observed: a 2000-node-cap run grew to >10GB RSS). A 64-bit hash
// costs a fixed ~8 bytes/entry regardless of program size; `maxEntries`
// additionally hard-caps total memory by resetting once the set gets too
// big, trading a bit of re-exploration for a real memory bound.
class Dedup {
public:
    explicit Dedup(size_t maxEntries = 2'000'000) : maxEntries_(maxEntries) {}

    // Returns true if `program` was not seen before (and records it).
    bool insertIfNew(const Stmt& program) {
        if (seen_.size() >= maxEntries_) seen_.clear();
        size_t key = std::hash<std::string>{}(program.toString());
        return seen_.insert(key).second;
    }

    bool contains(const Stmt& program) const { return seen_.count(std::hash<std::string>{}(program.toString())) > 0; }

    size_t size() const { return seen_.size(); }

private:
    size_t maxEntries_;
    std::unordered_set<size_t> seen_;
};

}  // namespace algosearch
