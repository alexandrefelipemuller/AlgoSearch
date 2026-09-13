#include <algorithm>

#include "problems/SortingProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(sorting_problem_generates_all_permutations_for_small_n) {
    SortingProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(3);  // N=3 -> 3! = 6 permutations, plus structural instances

    for (const auto& inst : instances) {
        std::vector<int> expected = inst.input;
        std::sort(expected.begin(), expected.end());
        CHECK_EQ(inst.expectedOutput, expected);
    }

    // All 6 permutations of {0,1,2} must be present among the instances.
    std::vector<std::vector<int>> inputs;
    for (const auto& inst : instances) inputs.push_back(inst.input);
    std::vector<int> perm = {0, 1, 2};
    int foundPermutations = 0;
    std::sort(perm.begin(), perm.end());
    do {
        if (std::find(inputs.begin(), inputs.end(), perm) != inputs.end()) foundPermutations++;
    } while (std::next_permutation(perm.begin(), perm.end()));
    CHECK_EQ(foundPermutations, 6);
}

TEST(sorting_problem_instances_include_structural_generalization_cases) {
    SortingProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(4);  // N=4

    bool hasDuplicates = false, hasNegative = false;
    for (const auto& inst : instances) {
        std::vector<int> sortedIn = inst.input;
        std::sort(sortedIn.begin(), sortedIn.end());
        if (std::adjacent_find(sortedIn.begin(), sortedIn.end()) != sortedIn.end()) hasDuplicates = true;
        for (int v : inst.input) {
            if (v < 0) hasNegative = true;
        }
    }
    CHECK(hasDuplicates);
    CHECK(hasNegative);
    // More than just the 24 permutations of {0,1,2,3}.
    CHECK(instances.size() > static_cast<size_t>(24));
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
    CHECK(instances.size() >= static_cast<size_t>(15));  // 15 random + structural generalization instances
    for (const auto& inst : instances) CHECK_EQ(inst.input.size(), static_cast<size_t>(5));
}

TEST(sorting_problem_next_level_increments) {
    SortingProblem problem;
    CHECK_EQ(problem.nextLevel(3), 4);
}

TEST(sorting_problem_partial_score_is_one_when_fully_sorted) {
    SortingProblem problem;
    CHECK_EQ(problem.partialScore({3, 1, 2}, {1, 2, 3}), 1.0);
}

TEST(sorting_problem_partial_score_penalizes_inversions) {
    SortingProblem problem;
    // {3,1,2} has 2 inversions out of a max of 3 -> 1 - 2/3
    double score = problem.partialScore({3, 1, 2}, {3, 1, 2});
    CHECK(score > 0.3 && score < 0.34);
}

TEST(sorting_problem_partial_score_zero_for_non_permutation) {
    SortingProblem problem;
    // Sorted-looking but not a rearrangement of the input -- must not get credit.
    CHECK_EQ(problem.partialScore({3, 1, 2}, {0, 0, 0}), 0.0);
}
