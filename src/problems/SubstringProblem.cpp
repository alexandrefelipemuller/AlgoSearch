#include "problems/SubstringProblem.hpp"

#include <algorithm>
#include <set>

#include "core/Random.hpp"

namespace algosearch {

SubstringProblem::SubstringProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {

bool containsSubstring(const std::vector<int>& text, const std::vector<int>& pattern) {
    int n = static_cast<int>(text.size());
    int m = static_cast<int>(pattern.size());
    if (m == 0) return true;
    if (m > n) return false;
    for (int i = 0; i + m <= n; ++i) {
        bool ok = true;
        for (int j = 0; j < m; ++j) {
            if (text[static_cast<size_t>(i) + j] != pattern[static_cast<size_t>(j)]) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
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
    return {std::move(input), {containsSubstring(text, pattern) ? 1 : 0}};
}

// A pattern of length `m` guaranteed NOT to occur in `text`. Tries
// within-alphabet patterns first (so "the pattern contains a value the text
// never uses" cannot become a shortcut for every absent case), falling back to
// an out-of-alphabet pattern only if the alphabet is so small every candidate
// occurs (all-equal text).
std::vector<int> absentPattern(const std::vector<int>& text, int m, int alphabetSize, Random& rng) {
    for (int attempt = 0; attempt < 200; ++attempt) {
        std::vector<int> pattern(static_cast<size_t>(m));
        for (int j = 0; j < m; ++j) pattern[static_cast<size_t>(j)] = rng.uniformInt(0, alphabetSize - 1);
        if (!containsSubstring(text, pattern)) return pattern;
    }
    std::vector<int> fallback(static_cast<size_t>(m));
    for (int j = 0; j < m; ++j) fallback[static_cast<size_t>(j)] = alphabetSize + 1 + j;
    return fallback;
}

// Adds present cases (the pattern is a real slice at front/middle/back) and
// one guaranteed-absent case for the same text; duplicates (e.g. all three
// positions slicing the same pattern out of an all-equal text) are collapsed.
void addBothCases(const std::vector<int>& text, int m, std::set<std::vector<int>>& seen, std::vector<Instance>& out,
                  Random& rng) {
    int n = static_cast<int>(text.size());
    if (m > n) return;
    int alphabetSize = 1;
    for (int v : text) alphabetSize = std::max(alphabetSize, v + 1);
    alphabetSize = std::max(2, alphabetSize);

    const int starts[3] = {0, std::max(0, n / 2 - m / 2), n - m};
    for (int start : starts) {
        if (start < 0 || start + m > n) continue;
        std::vector<int> pattern(text.begin() + start, text.begin() + start + m);
        auto input = makeInstance(text, pattern).input;
        if (seen.insert(input).second) out.push_back(makeInstance(text, pattern));
    }
    auto absent = absentPattern(text, m, alphabetSize, rng);
    auto input = makeInstance(text, absent).input;
    if (seen.insert(input).second) out.push_back(makeInstance(text, absent));
}

void addTextFamily(const std::vector<int>& text, int m, std::set<std::vector<int>>& seen, std::vector<Instance>& out,
                   Random& rng) {
    addBothCases(text, m, seen, out, rng);
}

std::vector<int> allEqualText(int n) { return std::vector<int>(static_cast<size_t>(n), 1); }

std::vector<int> alternatingText(int n) {
    std::vector<int> t(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) t[static_cast<size_t>(i)] = i % 2;
    return t;
}

std::vector<int> increasingText(int n) {
    std::vector<int> t(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) t[static_cast<size_t>(i)] = i;
    return t;
}

std::vector<int> randomText(int n, int alphabetSize, Random& rng) {
    std::vector<int> t(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) t[static_cast<size_t>(i)] = rng.uniformInt(0, alphabetSize - 1);
    return t;
}

}  // namespace

std::vector<Instance> SubstringProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    if (n <= 0) return instances;
    int m = std::max(1, (n + 1) / 2);

    std::set<std::vector<int>> seen;
    Random rng(seed_ + static_cast<uint64_t>(n) * 2654435761ULL);

    addTextFamily(allEqualText(n), m, seen, instances, rng);
    addTextFamily(alternatingText(n), m, seen, instances, rng);
    addTextFamily(increasingText(n), m, seen, instances, rng);

    int alphabet = std::max(1, std::min(4, n));
    // Below the exhaustive ceiling the crafted families already carry most of
    // the structure, so a handful of random texts suffices; above it the
    // random texts are the main variety, so sample more of them.
    int samples = n <= exhaustiveMaxN_ ? std::max(1, randomInstancesPerLevel_ / 6)
                                       : std::max(2, randomInstancesPerLevel_ / 4);
    for (int s = 0; s < samples; ++s) addTextFamily(randomText(n, alphabet, rng), m, seen, instances, rng);
    return instances;
}

std::string SubstringProblem::describeLevel(int level) const { return "text N = " + std::to_string(level); }

int SubstringProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> SubstringProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    int m = std::max(1, (level + 1) / 2);
    std::vector<Instance> instances;
    std::set<std::vector<int>> seen;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);

    // Wide-range text including negatives: same substring relation, but the
    // values are far outside anything generateInstances produced.
    for (int s = 0; s < 6; ++s) {
        std::vector<int> text(static_cast<size_t>(level));
        for (int i = 0; i < level; ++i) text[static_cast<size_t>(i)] = rng.uniformInt(-3 * level - 10, 3 * level + 10);
        addTextFamily(text, m, seen, instances, rng);
    }
    addTextFamily(allEqualText(level), m, seen, instances, rng);
    addTextFamily(alternatingText(level), m, seen, instances, rng);
    addTextFamily(increasingText(level), m, seen, instances, rng);
    return instances;
}

bool SubstringProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() < 2) return false;
    int n = input[0];
    if (n < 1 || static_cast<size_t>(n) + 1 >= input.size()) return false;
    int m = input[static_cast<size_t>(n) + 1];
    if (m < 1 || static_cast<size_t>(n) + 2 + m != input.size()) return false;
    std::vector<int> text(input.begin() + 1, input.begin() + 1 + n);
    std::vector<int> pattern(input.begin() + n + 2, input.end());
    return output[0] == (containsSubstring(text, pattern) ? 1 : 0);
}

}  // namespace algosearch
