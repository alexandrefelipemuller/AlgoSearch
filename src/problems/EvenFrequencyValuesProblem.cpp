#include "problems/EvenFrequencyValuesProblem.hpp"

#include <algorithm>

namespace algosearch {

int EvenFrequencyValuesProblem::answer(const std::vector<int>& values) const {
    int total = 0;
    for (size_t i = 0; i < values.size(); ++i) {
        auto here = values.begin() + static_cast<long>(i);
        if (std::find(values.begin(), here, values[i]) == here && std::count(values.begin(), values.end(), values[i]) % 2 == 0) total++;
    }
    return total;
}

std::vector<std::vector<int>> EvenFrequencyValuesProblem::constructedArrays(int n, Random& rng) const {
    // Small value range, so values repeat.
    std::vector<std::vector<int>> out;
    for (int c = 0; c < 15; ++c) {
        std::vector<int> v(static_cast<size_t>(n));
        for (int& x : v) x = rng.uniformInt(0, std::max(1, n / 3));
        out.push_back(v);
    }
    return out;
}

}  // namespace algosearch
