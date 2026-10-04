#include "problems/MaxRiseProblem.hpp"

#include <algorithm>

namespace algosearch {

int MaxRiseProblem::answer(const std::vector<int>& values) const {
    if (values.size() < 2) return 0;
    int best = values[1] - values[0];
    for (size_t i = 2; i < values.size(); ++i) best = std::max(best, values[i] - values[i - 1]);
    return best;
}

std::vector<std::vector<int>> MaxRiseProblem::constructedArrays(int n, Random& rng) const {
    // Arrays with long rises, drops and plateaus, values partly negative.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v;
        int x = rng.uniformInt(-n, n);
        while (static_cast<int>(v.size()) < n) {
            int step = rng.uniformInt(-n, n) * (c % 3);
            for (int k = 0; k < rng.uniformInt(1, 3) && static_cast<int>(v.size()) < n; ++k) v.push_back(x += step);
        }
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
