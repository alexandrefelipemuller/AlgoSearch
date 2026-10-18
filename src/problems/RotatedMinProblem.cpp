#include "problems/RotatedMinProblem.hpp"

#include <algorithm>

namespace algosearch {

namespace {

// Every rotation of strictly increasing sequences of size n built from `gaps` (cycled), starting at `base`, scaled.
void addRotations(int n, int base, const std::vector<int>& gaps, int scale, int shift, std::vector<std::vector<int>>& out) {
    std::vector<int> v(static_cast<size_t>(n));
    int x = base;
    for (int i = 0; i < n; ++i) {
        v[static_cast<size_t>(i)] = x * scale + shift;
        x += gaps[static_cast<size_t>(i) % gaps.size()];
    }
    for (int r = 0; r < n; ++r) {
        std::vector<int> rotated = v;
        std::rotate(rotated.begin(), rotated.begin() + r, rotated.end());
        out.push_back(std::move(rotated));
    }
}

}  // namespace

int RotatedMinProblem::answer(const std::vector<int>& values) const {
    return values.empty() ? 0 : *std::min_element(values.begin(), values.end());
}

std::vector<Instance> RotatedMinProblem::generateInstances(int level) const {
    std::vector<std::vector<int>> arrays;
    if (level <= 0) return {};
    for (int base : {0, 3}) {
        addRotations(level, base, {1}, 1, 0, arrays);
        addRotations(level, base, {2, 1, 3}, 1, 0, arrays);
    }
    std::vector<Instance> instances;
    for (auto& values : arrays) {
        int expected = answer(values);
        instances.push_back({std::move(values), {expected}});
    }
    return instances;
}

std::vector<Instance> RotatedMinProblem::generateAdversarialInstances(int level) const {
    std::vector<std::vector<int>> arrays;
    if (level <= 0) return {};
    // Every size up to `level` (a run starting at level 2 never trains on size 1), negatives, wide gaps.
    for (int n = 1; n <= level; ++n) {
        addRotations(n, -n, {1}, 7, -11, arrays);
        addRotations(n, -3, {5, 1, 2, 9}, 1, 0, arrays);
        addRotations(n, 100, {1}, 1, 0, arrays);
    }
    std::vector<Instance> instances;
    for (auto& values : arrays) {
        int expected = answer(values);
        instances.push_back({std::move(values), {expected}});
    }
    return instances;
}

}  // namespace algosearch
