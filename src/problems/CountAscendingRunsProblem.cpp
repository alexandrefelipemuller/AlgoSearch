#include "problems/CountAscendingRunsProblem.hpp"

#include <algorithm>

namespace algosearch {

int CountAscendingRunsProblem::answer(const std::vector<int>& values) const {
    int runs = values.empty() ? 0 : 1;
    for (size_t i = 1; i < values.size(); ++i) runs += values[i - 1] >= values[i] ? 1 : 0;
    return runs;
}

std::vector<std::vector<int>> CountAscendingRunsProblem::constructedArrays(int n, Random& rng) const {
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
