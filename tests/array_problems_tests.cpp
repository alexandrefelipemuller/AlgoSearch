#include <algorithm>

#include "problems/ContainsProblem.hpp"
#include "problems/MaxArrayProblem.hpp"
#include "problems/MinArrayProblem.hpp"
#include "problems/SumArrayProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

// ---- MaxArrayProblem ----

TEST(max_problem_is_solution_checks_maximum) {
    MaxArrayProblem problem;
    CHECK(problem.isSolution({3, 1, 2}, {3, 0, 0}));
    CHECK(!problem.isSolution({3, 1, 2}, {2, 0, 0}));
}

TEST(max_problem_partial_score_rewards_closeness) {
    MaxArrayProblem problem;
    CHECK_EQ(problem.partialScore({1, 5, 9}, {9, 0, 0}), 1.0);
    double near = problem.partialScore({1, 5, 9}, {8, 0, 0});
    CHECK(near > 0.8 && near < 0.9);
}

// ---- MinArrayProblem ----

TEST(min_problem_is_solution_checks_minimum) {
    MinArrayProblem problem;
    CHECK(problem.isSolution({3, 1, 2}, {1, 1, 1}));   // output[0] == min, rest ignored
    CHECK(!problem.isSolution({3, 1, 2}, {2, 0, 0}));  // wrong min
    CHECK(!problem.isSolution({3, 1, 2}, {1, 1}));     // wrong length
}

TEST(min_problem_partial_score_rewards_closeness) {
    MinArrayProblem problem;
    CHECK_EQ(problem.partialScore({1, 5, 9}, {1, 0, 0}), 1.0);  // exact
    double near = problem.partialScore({1, 5, 9}, {2, 0, 0});   // off by 1 out of range 8
    CHECK(near > 0.8 && near < 0.9);
    double far = problem.partialScore({1, 5, 9}, {9, 0, 0});  // off by 8 out of range 8
    CHECK(far >= 0.0 && far < 0.1);
}

TEST(min_problem_instances_include_structural_cases_and_are_correct) {
    MinArrayProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(4);
    CHECK(instances.size() > static_cast<size_t>(24));  // more than just the 24 permutations
    for (const auto& inst : instances) {
        int trueMin = *std::min_element(inst.input.begin(), inst.input.end());
        CHECK_EQ(inst.expectedOutput[0], trueMin);
    }
}

// ---- SumArrayProblem ----

TEST(sum_problem_is_solution_checks_total) {
    SumArrayProblem problem;
    CHECK(problem.isSolution({1, 2, 3}, {6, 0, 0}));
    CHECK(!problem.isSolution({1, 2, 3}, {7, 0, 0}));
}

TEST(sum_problem_partial_score_rewards_closeness) {
    SumArrayProblem problem;
    CHECK_EQ(problem.partialScore({1, 2, 3}, {6, 0, 0}), 1.0);
    double near = problem.partialScore({1, 2, 3}, {5, 0, 0});
    CHECK(near > 0.5 && near < 1.0);
}

TEST(sum_problem_handles_negatives) {
    SumArrayProblem problem;
    CHECK(problem.isSolution({-5, 3, -1}, {-3, 0, 0}));
}

// ---- ContainsProblem ----

TEST(contains_problem_detects_membership) {
    ContainsProblem problem;
    // input[0] = target, input[1..] = haystack
    CHECK(problem.isSolution({2, 5, 2, 9}, {1, 0, 0, 0}));   // 2 is present
    CHECK(problem.isSolution({7, 5, 2, 9}, {0, 0, 0, 0}));   // 7 is absent
    CHECK(!problem.isSolution({2, 5, 2, 9}, {0, 0, 0, 0}));  // wrong answer
}

TEST(contains_problem_generates_both_present_and_absent_cases) {
    ContainsProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(3);  // haystack length 3
    bool sawPresent = false, sawAbsent = false;
    for (const auto& inst : instances) {
        if (inst.expectedOutput[0] == 1) sawPresent = true;
        if (inst.expectedOutput[0] == 0) sawAbsent = true;
        CHECK_EQ(inst.input.size(), static_cast<size_t>(4));  // target + haystack of 3
    }
    CHECK(sawPresent);
    CHECK(sawAbsent);
}
