#include "problems/DominatedWindowProblem.hpp"

#include <algorithm>
#include <set>

#include "core/Random.hpp"

namespace algosearch {

DominatedWindowProblem::DominatedWindowProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars,
                                               uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars),
      seed_(seed) {}

namespace {

bool hasDominatedWindow(const std::vector<int>& text, const std::vector<int>& pattern) {
    int n = static_cast<int>(text.size());
    int m = static_cast<int>(pattern.size());
    if (m == 0) return true;
    if (m > n) return false;
    for (int i = 0; i + m <= n; ++i) {
        bool dominates = true;
        for (int j = 0; j < m; ++j) {
            if (text[static_cast<size_t>(i) + j] <= pattern[static_cast<size_t>(j)]) {
                dominates = false;
                break;
            }
        }
        if (dominates) return true;
    }
    return false;
}

Instance makeInstance(const std::vector<int>& text, const std::vector<int>& pattern) {
    int n = static_cast<int>(text.size());
    int m = static_cast<int>(pattern.size());
    std::vector<int> input;
    input.reserve(static_cast<size_t>(n) + m + 2);
    input.push_back(n);
    input.insert(input.end(), text.begin(), text.end());
    input.push_back(m);
    input.insert(input.end(), pattern.begin(), pattern.end());
    return {std::move(input), {hasDominatedWindow(text, pattern) ? 1 : 0}};
}

// Deterministically dominated: pattern[j] = text[start+j] - 1, so window
// `start` dominates by construction.
std::vector<int> dominatedPattern(const std::vector<int>& text, int m, int start) {
    std::vector<int> pattern(static_cast<size_t>(m));
    for (int j = 0; j < m; ++j) pattern[static_cast<size_t>(j)] = text[static_cast<size_t>(start) + j] - 1;
    return pattern;
}

// Deterministically never dominated: every pattern element exceeds every
// text element, so no window can dominate it anywhere.
std::vector<int> neverDominatedPattern(const std::vector<int>& text, int m) {
    int maxVal = text.empty() ? 0 : *std::max_element(text.begin(), text.end());
    return std::vector<int>(static_cast<size_t>(m), maxVal + 1);
}

void addBothCases(const std::vector<int>& text, int m, std::set<std::vector<int>>& seen, std::vector<Instance>& out,
                  Random& rng) {
    int n = static_cast<int>(text.size());
    if (m > n) return;

    const int starts[3] = {0, std::max(0, n / 2 - m / 2), n - m};
    for (int start : starts) {
        if (start < 0 || start + m > n) continue;
        auto pattern = dominatedPattern(text, m, start);
        auto input = makeInstance(text, pattern).input;
        if (seen.insert(input).second) out.push_back(makeInstance(text, pattern));
    }
    auto absent = neverDominatedPattern(text, m);
    auto input = makeInstance(text, absent).input;
    if (seen.insert(input).second) out.push_back(makeInstance(text, absent));
    (void)rng;
}

std::vector<int> allEqualText(int n) { return std::vector<int>(static_cast<size_t>(n), 5); }

std::vector<int> increasingText(int n) {
    std::vector<int> t(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) t[static_cast<size_t>(i)] = i;
    return t;
}

std::vector<int> alternatingText(int n) {
    std::vector<int> t(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) t[static_cast<size_t>(i)] = (i % 2 == 0) ? 5 : 1;
    return t;
}

std::vector<int> randomText(int n, int lo, int hi, Random& rng) {
    std::vector<int> t(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) t[static_cast<size_t>(i)] = rng.uniformInt(lo, hi);
    return t;
}

}  // namespace

std::vector<Instance> DominatedWindowProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    if (n <= 0) return instances;
    int m = std::max(1, (n + 1) / 2);

    std::set<std::vector<int>> seen;
    Random rng(seed_ + static_cast<uint64_t>(n) * 2654435761ULL);

    addBothCases(allEqualText(n), m, seen, instances, rng);
    addBothCases(increasingText(n), m, seen, instances, rng);
    addBothCases(alternatingText(n), m, seen, instances, rng);

    int samples = n <= exhaustiveMaxN_ ? std::max(1, randomInstancesPerLevel_ / 6)
                                       : std::max(2, randomInstancesPerLevel_ / 4);
    for (int s = 0; s < samples; ++s) addBothCases(randomText(n, 1, 10, rng), m, seen, instances, rng);
    return instances;
}

std::string DominatedWindowProblem::describeLevel(int level) const { return "text N = " + std::to_string(level); }

int DominatedWindowProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> DominatedWindowProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    int m = std::max(1, (level + 1) / 2);
    std::vector<Instance> instances;
    std::set<std::vector<int>> seen;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);

    for (int s = 0; s < 6; ++s) {
        addBothCases(randomText(level, -3 * level - 10, 3 * level + 10, rng), m, seen, instances, rng);
    }
    addBothCases(allEqualText(level), m, seen, instances, rng);
    addBothCases(alternatingText(level), m, seen, instances, rng);
    addBothCases(increasingText(level), m, seen, instances, rng);
    return instances;
}

bool DominatedWindowProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() < 2) return false;
    int n = input[0];
    if (n < 1 || static_cast<size_t>(n) + 1 >= input.size()) return false;
    int m = input[static_cast<size_t>(n) + 1];
    if (m < 1 || static_cast<size_t>(n) + 2 + m != input.size()) return false;
    std::vector<int> text(input.begin() + 1, input.begin() + 1 + n);
    std::vector<int> pattern(input.begin() + n + 2, input.end());
    return output[0] == (hasDominatedWindow(text, pattern) ? 1 : 0);
}

}  // namespace algosearch
