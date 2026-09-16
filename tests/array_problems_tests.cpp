#include <algorithm>

#include "problems/AverageProblem.hpp"
#include "problems/ContainsProblem.hpp"
#include "problems/CountProblem.hpp"
#include "problems/MaxArrayProblem.hpp"
#include "problems/MinArrayProblem.hpp"
#include "problems/ReverseProblem.hpp"
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

// ---- CountProblem ----

TEST(count_problem_counts_elements_above_threshold) {
    CountProblem problem;
    // input[0] = threshold, input[1..] = haystack
    CHECK(problem.isSolution({2, 5, 2, 9}, {2, 0, 0, 0}));   // 5 and 9 are > 2
    CHECK(problem.isSolution({9, 5, 2, 9}, {0, 0, 0, 0}));   // nothing is > 9
    CHECK(!problem.isSolution({2, 5, 2, 9}, {1, 0, 0, 0}));  // wrong count
}

TEST(count_problem_partial_score_rewards_closeness) {
    CountProblem problem;
    CHECK_EQ(problem.partialScore({2, 5, 2, 9}, {2, 0, 0, 0}), 1.0);
    double near = problem.partialScore({2, 5, 2, 9}, {1, 0, 0, 0});
    CHECK(near > 0.5 && near < 1.0);
}

TEST(count_problem_instances_cover_zero_partial_and_full_counts) {
    CountProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(3);
    bool sawZero = false, sawFull = false;
    for (const auto& inst : instances) {
        int threshold = inst.input[0];
        std::vector<int> haystack(inst.input.begin() + 1, inst.input.end());
        int trueCount = 0;
        for (int v : haystack) {
            if (v > threshold) trueCount++;
        }
        CHECK_EQ(inst.expectedOutput[0], trueCount);
        if (trueCount == 0) sawZero = true;
        if (trueCount == static_cast<int>(haystack.size())) sawFull = true;
    }
    CHECK(sawZero);
    CHECK(sawFull);
}

// ---- AverageProblem ----

TEST(average_problem_is_solution_checks_integer_average) {
    AverageProblem problem;
    CHECK(problem.isSolution({1, 2, 3}, {2, 0, 0}));   // (1+2+3)/3 == 2
    CHECK(problem.isSolution({1, 2, 4}, {2, 0, 0}));   // 7/3 truncates to 2
    CHECK(!problem.isSolution({1, 2, 3}, {3, 0, 0}));  // wrong average
}

TEST(average_problem_partial_score_rewards_closeness) {
    AverageProblem problem;
    CHECK_EQ(problem.partialScore({1, 2, 3}, {2, 0, 0}), 1.0);
    double near = problem.partialScore({1, 2, 3}, {3, 0, 0});
    CHECK(near > 0.5 && near < 1.0);
}

TEST(average_problem_instances_are_non_negative_and_correct) {
    AverageProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(4);
    for (const auto& inst : instances) {
        long sum = 0;
        for (int v : inst.input) {
            CHECK(v >= 0);  // domain restriction: division-via-subtraction needs non-negative operands
            sum += v;
        }
        CHECK_EQ(inst.expectedOutput[0], static_cast<int>(sum / static_cast<long>(inst.input.size())));
    }
}

// ---- ReverseProblem ----

TEST(reverse_problem_is_solution_checks_reversed_order) {
    ReverseProblem problem;
    CHECK(problem.isSolution({1, 2, 3}, {3, 2, 1}));
    CHECK(!problem.isSolution({1, 2, 3}, {1, 2, 3}));  // not reversed (unless a palindrome)
    CHECK(problem.isSolution({5}, {5}));               // single element reverses to itself
}

TEST(reverse_problem_partial_score_counts_matching_positions) {
    ReverseProblem problem;
    CHECK_EQ(problem.partialScore({1, 2, 3, 4}, {4, 3, 2, 1}), 1.0);
    double half = problem.partialScore({1, 2, 3, 4}, {4, 3, 3, 1});  // wrong value entirely -> not a rearrangement
    CHECK_EQ(half, 0.0);
    double near = problem.partialScore({1, 2, 3, 4}, {4, 2, 3, 1});  // rearrangement, 2/4 positions correct
    CHECK_EQ(near, 0.5);
}

TEST(reverse_problem_instances_are_correct) {
    ReverseProblem problem(/*exhaustiveMaxN=*/6, 40, 4, 1);
    auto instances = problem.generateInstances(4);
    for (const auto& inst : instances) {
        std::vector<int> expected(inst.input.rbegin(), inst.input.rend());
        CHECK(inst.expectedOutput == expected);
    }
}
