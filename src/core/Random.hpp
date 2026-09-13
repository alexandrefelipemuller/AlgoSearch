#pragma once

#include <cstdint>
#include <random>

namespace algosearch {

// Thin wrapper around a seeded PRNG so search/generation is reproducible.
class Random {
public:
    explicit Random(uint64_t seed) : engine_(seed) {}

    // Inclusive range [lo, hi].
    int uniformInt(int lo, int hi) {
        if (lo >= hi) return lo;
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(engine_);
    }

    bool chance(double probability) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(engine_) < probability;
    }

    std::mt19937_64& engine() { return engine_; }

private:
    std::mt19937_64 engine_;
};

}  // namespace algosearch
