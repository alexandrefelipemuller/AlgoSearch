#include <algorithm>

#include "problems/SortingProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(sorting_problem_generates_all_permutations_for_small_n) {
    SortingProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(3);  // N=3 -> 3! = 6 permutations
    CHECK_EQ(instances.size(), static_cast<size_t>(6));

    for (const auto& inst : instances) {
        std::vector<int> expected = inst.input;
        std::sort(expected.begin(), expected.end());
        CHECK_EQ(inst.expectedOutput, expected);
    }

    // All permutations must be distinct.
    std::vector<std::vector<int>> inputs;
    for (const auto& inst : instances) inputs.push_back(inst.input);
    std::sort(inputs.begin(), inputs.end());
    CHECK(std::unique(inputs.begin(), inputs.end()) == inputs.end());
}

TEST(sorting_problem_level_one_is_trivial) {
    SortingProblem problem;
    auto instances = problem.generateInstances(1);
    CHECK_EQ(instances.size(), static_cast<size_t>(1));
    CHECK_EQ(instances[0].input, (std::vector<int>{0}));
}

TEST(sorting_problem_is_solution_checks_sortedness) {
    SortingProblem problem;
    CHECK(problem.isSolution({3, 1, 2}, {1, 2, 3}));
    CHECK(!problem.isSolution({3, 1, 2}, {3, 1, 2}));
    CHECK(!problem.isSolution({3, 1, 2}, {1, 2}));
}

TEST(sorting_problem_falls_back_to_random_beyond_exhaustive_threshold) {
    SortingProblem problem(/*exhaustiveMaxN=*/3, /*randomInstancesPerLevel=*/15, 4, 7);
    auto instances = problem.generateInstances(5);  // beyond exhaustiveMaxN
    CHECK_EQ(instances.size(), static_cast<size_t>(15));
    for (const auto& inst : instances) CHECK_EQ(inst.input.size(), static_cast<size_t>(5));
}

TEST(sorting_problem_next_level_increments) {
    SortingProblem problem;
    CHECK_EQ(problem.nextLevel(3), 4);
}
