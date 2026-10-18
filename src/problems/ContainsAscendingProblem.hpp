#pragma once

#include "problems/ContainsOrderedProblem.hpp"

namespace algosearch {

// Section 121c: contains_ordered restricted to ascending haystacks. The both-directions version needs a direction
// test that no single comparison supplies; this one asks whether the search piece finds a key-based bisection.
class ContainsAscendingProblem : public ContainsOrderedProblem {
public:
    using ContainsOrderedProblem::ContainsOrderedProblem;
    std::vector<Instance> generateInstances(int level) const override;
    std::vector<Instance> generateAdversarialInstances(int level) const override;
    std::string describeLevel(int level) const override;
};

}  // namespace algosearch
