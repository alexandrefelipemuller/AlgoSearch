#include "problems/ContainsAscendingProblem.hpp"

#include <algorithm>

namespace algosearch {

namespace {

std::vector<Instance> keepAscending(std::vector<Instance> all) {
    std::vector<Instance> out;
    for (auto& inst : all) {
        if (inst.input.size() >= 2 && std::is_sorted(inst.input.begin() + 1, inst.input.end())) out.push_back(std::move(inst));
    }
    return out;
}

}  // namespace

std::vector<Instance> ContainsAscendingProblem::generateInstances(int level) const {
    return keepAscending(ContainsOrderedProblem::generateInstances(level));
}

std::vector<Instance> ContainsAscendingProblem::generateAdversarialInstances(int level) const {
    return keepAscending(ContainsOrderedProblem::generateAdversarialInstances(level));
}

std::string ContainsAscendingProblem::describeLevel(int level) const { return "ascending haystack N = " + std::to_string(level); }

}  // namespace algosearch
