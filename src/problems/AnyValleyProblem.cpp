#include "problems/AnyValleyProblem.hpp"

namespace algosearch {

namespace {

bool isValley(const std::vector<int>& values, int i) {
    int n = static_cast<int>(values.size());
    return (i == 0 || values[i] <= values[i - 1]) && (i == n - 1 || values[i] <= values[i + 1]);
}

}  // namespace

int AnyValleyProblem::answer(const std::vector<int>& values) const {
    for (int i = 0; i < static_cast<int>(values.size()); ++i) {
        if (isValley(values, i)) return i;
    }
    return 0;
}

bool AnyValleyProblem::isSolution(const std::vector<int>& input, const std::vector<int>& output) const {
    if (input.empty() || output.size() != input.size()) return false;
    int i = output[0];
    return i >= 0 && i < static_cast<int>(input.size()) && isValley(input, i);
}

std::vector<std::vector<int>> AnyValleyProblem::constructedArrays(int n, Random& rng) const {
    // Wide random arrays, so peaks sit away from the ends and several peaks coexist.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, 2 * n);
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
