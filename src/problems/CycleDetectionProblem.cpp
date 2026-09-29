#include "problems/CycleDetectionProblem.hpp"

#include "problems/ArrayInstances.hpp"

#include <set>

#include "core/Random.hpp"

namespace algosearch {

CycleDetectionProblem::CycleDetectionProblem(int exhaustiveMaxN, int randomInstancesPerLevel, int numVars, uint64_t seed)
    : exhaustiveMaxN_(exhaustiveMaxN), randomInstancesPerLevel_(randomInstancesPerLevel), numVars_(numVars), seed_(seed) {}

namespace {

// Host-side ground truth: walk the list from head; a revisit before null
// means a cycle. Valid instances always terminate (at most n+1 distinct
// cursor positions before either outcome).
bool listHasCycle(const std::vector<int>& input) {
    int n = static_cast<int>(input.size()) - 1;
    int head = input[0];
    if (head < 0 || head > n) return false;
    std::vector<char> seen(static_cast<size_t>(n) + 1, 0);
    int cur = head;
    while (cur != n) {
        if (seen[static_cast<size_t>(cur)]) return true;
        seen[static_cast<size_t>(cur)] = 1;
        cur = input[static_cast<size_t>(1) + cur];
    }
    return false;
}

// Assembles [head, next pointers...] with the expected output.
Instance makeInstance(int head, const std::vector<int>& next) {
    std::vector<int> input;
    input.reserve(next.size() + 1);
    input.push_back(head);
    input.insert(input.end(), next.begin(), next.end());
    int answer = listHasCycle(input) ? 1 : 0;  // computed BEFORE the move below
    return {std::move(input), {answer}};
}

void addInstance(int head, const std::vector<int>& next, std::set<std::vector<int>>& seen, std::vector<Instance>& out) {
    auto inst = makeInstance(head, next);
    if (seen.insert(inst.input).second) out.push_back(std::move(inst));
}

std::vector<int> linearNext(int n) {
    std::vector<int> next(static_cast<size_t>(n));
    for (int i = 0; i + 1 < n; ++i) next[static_cast<size_t>(i)] = i + 1;
    if (n > 0) next[static_cast<size_t>(n) - 1] = n;  // null
    return next;
}

void addCraftedInstances(int n, std::set<std::vector<int>>& seen, std::vector<Instance>& out) {
    if (n < 1) return;
    addInstance(0, linearNext(n), seen, out);  // plain acyclic chain

    {  // tail then cycle: the classic positive case
        std::vector<int> next = linearNext(n);
        int cycleStart = n / 2;
        next[static_cast<size_t>(n) - 1] = cycleStart;
        addInstance(0, next, seen, out);
    }
    {  // pure cycle covering every node
        std::vector<int> next(static_cast<size_t>(n));
        for (int i = 0; i < n; ++i) next[static_cast<size_t>(i)] = (i + 1) % n;
        addInstance(0, next, seen, out);
    }
    {  // self-loop at the head (same full n-node layout as every other case)
        std::vector<int> next = linearNext(n);
        next[0] = 0;
        addInstance(0, next, seen, out);
    }

    if (n >= 4) {
        // A cycle that exists in the graph but is NOT reachable from head:
        // guards against a shortcut that checks "does any cycle exist anywhere"
        // instead of tracing the actual walk from head.
        std::vector<int> next(static_cast<size_t>(n));
        next[0] = 1;
        next[1] = n;  // null: the head's walk ends immediately
        for (int i = 2; i + 1 < n; ++i) next[static_cast<size_t>(i)] = i + 1;
        next[static_cast<size_t>(n) - 1] = 2;  // closes a cycle over nodes 2..n-1
        addInstance(0, next, seen, out);
    }
    if (n >= 2) {
        // Acyclic, but the head is NOT node 0 -- guards against hardcoding
        // "start at node 0".
        addInstance(n / 2, linearNext(n), seen, out);
    }
    addInstance(n, linearNext(n), seen, out);  // empty list (head = null)
}

std::vector<int> randomNext(int n, Random& rng, bool reachableCycle, bool unreachableCycle) {
    std::vector<int> next = linearNext(n);
    if (reachableCycle && n >= 1) {
        int cycleStart = rng.uniformInt(0, n - 1);
        next[static_cast<size_t>(n) - 1] = cycleStart;
    }
    if (unreachableCycle && n >= 4) {
        // Keep the head's short acyclic walk, but add a cycle over a suffix
        // the walk never reaches (same shape as the crafted guard case).
        int suffixStart = 2;
        for (int i = suffixStart; i + 1 < n; ++i) next[static_cast<size_t>(i)] = i + 1;
        next[static_cast<size_t>(n) - 1] = suffixStart;
    }
    return next;
}

}  // namespace

std::vector<Instance> CycleDetectionProblem::generateInstances(int level) const {
    int n = level;
    std::vector<Instance> instances;
    if (n <= 0) return instances;

    std::set<std::vector<int>> seen;
    addCraftedInstances(n, seen, instances);

    Random rng(seed_ + static_cast<uint64_t>(n) * 2654435761ULL);
    int samples = n <= exhaustiveMaxN_ ? std::max(1, randomInstancesPerLevel_ / 3)
                                       : std::max(2, randomInstancesPerLevel_ / 2);
    for (int s = 0; s < samples; ++s) {
        int kind = s % 3;
        bool reachableCycle = kind == 1;
        bool unreachableCycle = kind == 2;
        auto next = randomNext(n, rng, reachableCycle, unreachableCycle);
        int head = (s % 4 == 3) ? rng.uniformInt(0, n) : 0;
        addInstance(head, next, seen, instances);
    }
    return instances;
}

std::string CycleDetectionProblem::describeLevel(int level) const { return "nodes N = " + std::to_string(level); }

int CycleDetectionProblem::nextLevel(int level) const { return level + 1; }

std::vector<Instance> CycleDetectionProblem::generateAdversarialInstances(int level) const {
    if (level <= 0) return {};
    std::vector<Instance> instances;
    std::set<std::vector<int>> seen;
    Random rng(seed_ + static_cast<uint64_t>(level) * 999983ULL + 11);

    addCraftedInstances(level, seen, instances);
    for (int s = 0; s < 10; ++s) {
        bool reachableCycle = s % 2 == 1;
        bool unreachableCycle = s % 5 == 4;
        auto next = randomNext(level, rng, reachableCycle, unreachableCycle);
        int head = (s % 3 == 2) ? rng.uniformInt(0, level) : 0;
        addInstance(head, next, seen, instances);
    }
    return instances;
}

bool CycleDetectionProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (output.size() != input.size() || input.size() < 2) return false;
    int n = static_cast<int>(input.size()) - 1;
    int head = input[0];
    if (head < 0 || head > n) return false;
    // Every next pointer must stay in [0..n] for the encoding to be valid.
    for (int i = 1; i <= n; ++i) {
        if (input[static_cast<size_t>(i)] < 0 || input[static_cast<size_t>(i)] > n) return false;
    }
    return output[0] == (listHasCycle(input) ? 1 : 0);
}

// Every (head, next[]) with head and each next in [0, n] -- all functional
// graphs on n nodes plus null -- for n <= 5 ((n+1)^(n+1) inputs, 46,656 at
// n = 5). Section 38 found a "has a repeated value" program that passes the
// sampled benchmark here while wrong on ~45% of these.
std::vector<std::vector<int>> CycleDetectionProblem::exhaustiveInputs(int n) const {
    if (n < 1 || n > 5) return {};
    return allArraysOver(n + 1, 0, n);
}

}  // namespace algosearch
