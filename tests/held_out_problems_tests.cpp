#include <algorithm>
#include <cmath>
#include <set>

#include "problems/CycleDetectionProblem.hpp"
#include "problems/LisProblem.hpp"
#include "problems/SubstringProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

int hostLisLength(const std::vector<int>& values) {
    if (values.empty()) return 0;
    std::vector<int> dp(values.size(), 1);
    int best = 1;
    for (size_t i = 0; i < values.size(); ++i) {
        for (size_t j = 0; j < i; ++j) {
            if (values[j] < values[i]) dp[i] = std::max(dp[i], dp[j] + 1);
        }
        best = std::max(best, dp[i]);
    }
    return best;
}

bool hostContainsSubstring(const std::vector<int>& text, const std::vector<int>& pattern) {
    int n = static_cast<int>(text.size());
    int m = static_cast<int>(pattern.size());
    if (m == 0) return true;
    if (m > n) return false;
    for (int i = 0; i + m <= n; ++i) {
        bool ok = true;
        for (int j = 0; j < m; ++j) {
            if (text[static_cast<size_t>(i) + j] != pattern[static_cast<size_t>(j)]) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}

bool hostListHasCycle(const std::vector<int>& input) {
    int n = static_cast<int>(input.size()) - 1;
    int head = input[0];
    if (head < 0 || head > n) return false;
    std::vector<char> seen(static_cast<size_t>(n) + 1, 0);
    int cur = head;
    while (cur != n) {
        if (seen[static_cast<size_t>(cur)]) return true;
        seen[static_cast<size_t>(cur)] = 1;
        cur = input[static_cast<size_t>(1) + cur];
    }
    return false;
}

}  // namespace

// ---- LisProblem ----

TEST(lis_problem_is_solution_checks_lis_length) {
    LisProblem problem;
    // [n=3, values 1 2 3, scratch 0 0 0] -> LIS = 3
    CHECK(problem.isSolution({3, 1, 2, 3, 0, 0, 0}, {3, 0, 0, 0, 0, 0, 0}));
    CHECK(!problem.isSolution({3, 1, 2, 3, 0, 0, 0}, {2, 0, 0, 0, 0, 0, 0}));
    CHECK(!problem.isSolution({3, 1, 2, 3, 0, 0, 0}, {3, 0, 0}));  // wrong length
}

TEST(lis_problem_handles_decreasing_and_equal_sequences) {
    LisProblem problem;
    // [n=3, values 3 2 1] -> LIS = 1
    CHECK(problem.isSolution({3, 3, 2, 1, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0}));
    // [n=3, values 1 1 1] -> LIS = 1
    CHECK(problem.isSolution({3, 1, 1, 1, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0}));
}

TEST(lis_problem_partial_score_rewards_closeness) {
    LisProblem problem;
    // [n=2, values 1 2] -> LIS = 2
    CHECK_EQ(problem.partialScore({2, 1, 2, 0, 0}, {2, 0, 0, 0, 0}), 1.0);
    double near = problem.partialScore({2, 1, 2, 0, 0}, {1, 0, 0, 0, 0});
    CHECK(near > 0.4 && near < 0.6);
}

TEST(lis_problem_instances_have_valid_layout_and_correct_answers) {
    LisProblem problem(/*exhaustiveMaxN=*/4, 30, 6, 1);
    for (int level = 1; level <= 5; ++level) {
        auto instances = problem.generateInstances(level);
        CHECK(instances.size() > 0);
        for (const auto& inst : instances) {
            int n = inst.input[0];
            CHECK_EQ(n, level);
            CHECK_EQ(inst.input.size(), static_cast<size_t>(2) * n + 1);
            for (int i = n + 1; i <= 2 * n; ++i) CHECK_EQ(inst.input[static_cast<size_t>(i)], 0);
            std::vector<int> values(inst.input.begin() + 1, inst.input.begin() + 1 + n);
            CHECK_EQ(inst.expectedOutput[0], hostLisLength(values));
        }
    }
}

// ---- SubstringProblem ----

TEST(substring_problem_is_solution_checks_occurrence) {
    SubstringProblem problem;
    // text = [0 1 2], pattern = [1 2] -> present
    CHECK(problem.isSolution({3, 0, 1, 2, 2, 1, 2}, {1, 0, 0, 0, 0, 0, 0}));
    // text = [0 1 2], pattern = [2 0] -> absent
    CHECK(problem.isSolution({3, 0, 1, 2, 2, 2, 0}, {0, 0, 0, 0, 0, 0, 0}));
    CHECK(!problem.isSolution({3, 0, 1, 2, 2, 2, 0}, {1, 0, 0, 0, 0, 0, 0}));
    CHECK(!problem.isSolution({3, 0, 1, 2, 2, 1, 2}, {1, 0, 0}));  // wrong length
}

TEST(substring_problem_instances_cover_both_labels_with_correct_answers) {
    SubstringProblem problem(/*exhaustiveMaxN=*/5, 30, 6, 1);
    for (int level = 1; level <= 6; ++level) {
        auto instances = problem.generateInstances(level);
        CHECK(instances.size() > 0);
        int present = 0, absent = 0;
        for (const auto& inst : instances) {
            int n = inst.input[0];
            CHECK_EQ(n, level);
            CHECK(inst.input.size() >= static_cast<size_t>(2) + 2);
            int m = inst.input[static_cast<size_t>(n) + 1];
            CHECK_EQ(inst.input.size(), static_cast<size_t>(n) + m + 2);
            std::vector<int> text(inst.input.begin() + 1, inst.input.begin() + 1 + n);
            std::vector<int> pattern(inst.input.begin() + n + 2, inst.input.end());
            bool occurs = hostContainsSubstring(text, pattern);
            CHECK_EQ(inst.expectedOutput[0], occurs ? 1 : 0);
            if (occurs)
                present++;
            else
                absent++;
        }
        CHECK(present > 0);
        CHECK(absent > 0);
    }
}

// ---- CycleDetectionProblem ----

TEST(cycle_problem_is_solution_checks_walk_from_head) {
    CycleDetectionProblem problem;
    // head=0, next=[1,2,3] (3 = null): acyclic chain 0->1->2->null
    CHECK(problem.isSolution({0, 1, 2, 3}, {0, 1, 2, 3}));
    // head=0, next=[1,0]: 0->1->0 cycle
    CHECK(problem.isSolution({0, 1, 0}, {1, 0, 0}));
    CHECK(!problem.isSolution({0, 1, 0}, {0, 0, 0}));
    // head=0, next=[1,4,3,2]: walk 0->1->null ends, though 2<->3 cycles unreachably
    CHECK(problem.isSolution({0, 1, 4, 3, 2}, {0, 0, 0, 0, 0}));
}

TEST(cycle_problem_empty_head_is_acyclic) {
    CycleDetectionProblem problem;
    // head = 3 = null (empty list), any next pointers are irrelevant
    CHECK(problem.isSolution({3, 1, 2, 3}, {0, 1, 2, 3}));
}

TEST(cycle_problem_instances_have_valid_layout_and_correct_answers) {
    CycleDetectionProblem problem(/*exhaustiveMaxN=*/6, 30, 5, 1);
    for (int level = 1; level <= 7; ++level) {
        auto instances = problem.generateInstances(level);
        CHECK(instances.size() > 0);
        int withCycle = 0, withoutCycle = 0;
        for (const auto& inst : instances) {
            CHECK_EQ(inst.input.size(), static_cast<size_t>(level) + 1);
            CHECK(inst.input[0] >= 0 && inst.input[0] <= level);
            for (int i = 1; i <= level; ++i) {
                CHECK(inst.input[static_cast<size_t>(i)] >= 0 && inst.input[static_cast<size_t>(i)] <= level);
            }
            bool cycle = hostListHasCycle(inst.input);
            CHECK_EQ(inst.expectedOutput[0], cycle ? 1 : 0);
            if (cycle)
                withCycle++;
            else
                withoutCycle++;
        }
        CHECK(withCycle > 0);
        CHECK(withoutCycle > 0);
    }
}

TEST(held_out_problems_next_level_increments) {
    LisProblem lis;
    SubstringProblem sub;
    CycleDetectionProblem cyc;
    CHECK_EQ(lis.nextLevel(3), 4);
    CHECK_EQ(sub.nextLevel(3), 4);
    CHECK_EQ(cyc.nextLevel(3), 4);
}
