#include <vector>

#include "problems/EquilibriumPointProblem.hpp"
#include "problems/PeakElementProblem.hpp"
#include "problems/SearchRotatedProblem.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(section_114_problems_compute_their_answers) {
    CHECK_EQ(PeakElementProblem().answer({1, 3, 2, 5}), 1);
    CHECK_EQ(PeakElementProblem().answer({4, 1}), 0);
    CHECK_EQ(PeakElementProblem().answer({}), 0);
    CHECK_EQ(SearchRotatedProblem().answer({5, 7, 9, 1, 3, 5}), 5);
    CHECK_EQ(SearchRotatedProblem().answer({4, 7, 9, 1, 3}), -1);
    CHECK_EQ(EquilibriumPointProblem().answer({1, 3, 5, 2, 2}), 2);
    CHECK_EQ(EquilibriumPointProblem().answer({1, 2}), -1);
    CHECK_EQ(EquilibriumPointProblem().answer({}), -1);
}

TEST(section_114_instances_are_labelled_by_answer) {
    PeakElementProblem peak;
    SearchRotatedProblem search;
    EquilibriumPointProblem balance;
    for (int level = 1; level <= 6; ++level) {
        for (const ScalarFromArrayProblem* p : std::vector<const ScalarFromArrayProblem*>{&peak, &search, &balance}) {
            auto instances = p->generateInstances(level);
            CHECK(!instances.empty());
            for (const auto& inst : instances) {
                std::vector<int> out = inst.input;
                if (!out.empty()) out[0] = inst.expectedOutput[0];
                else out.push_back(inst.expectedOutput[0]);
                CHECK(p->isSolution(inst.input, out));
            }
        }
    }
}

TEST(section_114_constructed_instances_hit_the_rare_labels) {
    // search_rotated finds the key and equilibrium_point balances in some instances.
    int found = 0, balanced = 0;
    for (const auto& inst : SearchRotatedProblem().generateInstances(6)) found += inst.expectedOutput[0] > 0;
    for (const auto& inst : EquilibriumPointProblem().generateInstances(6)) balanced += inst.expectedOutput[0] >= 0;
    CHECK(found > 0);
    CHECK(balanced > 0);
}
