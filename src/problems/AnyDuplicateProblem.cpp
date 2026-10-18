#include "problems/AnyDuplicateProblem.hpp"

namespace algosearch {

namespace {

int occurrences(const std::vector<int>& values, int v) {
    int count = 0;
    for (int x : values) count += x == v;
    return count;
}

bool hasDuplicate(const std::vector<int>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) return true;
        }
    }
    return false;
}

}  // namespace

int AnyDuplicateProblem::answer(const std::vector<int>& values) const {
    for (size_t i = 0; i < values.size(); ++i) {
        if (occurrences(values, values[i]) >= 2) return values[i];
    }
    return -1;
}

bool AnyDuplicateProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (input.empty() || output.size() != input.size()) return false;
    if (!hasDuplicate(input)) return output[0] == -1;
    return occurrences(input, output[0]) >= 2;
}

std::vector<std::vector<int>> AnyDuplicateProblem::constructedArrays(int n, Random& rng) const {
    // Narrow value range plus a planted pair, so duplicates are common and often several values repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, n);
        if (n >= 2 && c % 3 == 0) v[static_cast<size_t>(rng.uniformInt(0, n - 1))] = v[0];
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
