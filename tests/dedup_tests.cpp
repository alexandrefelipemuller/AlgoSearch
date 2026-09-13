#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "search/Dedup.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(dedup_rejects_structurally_identical_programs) {
    Dedup dedup;

    auto makeProgram = []() {
        auto block = makeBlock();
        block->body.push_back(makeAssign(0, makeIntConst(1)));
        return block;
    };

    auto p1 = makeProgram();
    auto p2 = makeProgram();  // structurally identical, different objects

    CHECK(dedup.insertIfNew(*p1));
    CHECK(!dedup.insertIfNew(*p2));
    CHECK_EQ(dedup.size(), static_cast<size_t>(1));
}

TEST(dedup_accepts_structurally_different_programs) {
    Dedup dedup;

    auto p1 = makeBlock();
    p1->body.push_back(makeAssign(0, makeIntConst(1)));

    auto p2 = makeBlock();
    p2->body.push_back(makeAssign(0, makeIntConst(2)));

    CHECK(dedup.insertIfNew(*p1));
    CHECK(dedup.insertIfNew(*p2));
    CHECK_EQ(dedup.size(), static_cast<size_t>(2));
}

TEST(dedup_caps_memory_by_resetting_past_max_entries) {
    Dedup dedup(/*maxEntries=*/3);

    for (int i = 0; i < 3; ++i) {
        auto p = makeBlock();
        p->body.push_back(makeAssign(0, makeIntConst(i)));
        CHECK(dedup.insertIfNew(*p));
    }
    CHECK_EQ(dedup.size(), static_cast<size_t>(3));

    // One more distinct entry pushes past the cap: the set resets instead
    // of growing without bound.
    auto p4 = makeBlock();
    p4->body.push_back(makeAssign(0, makeIntConst(999)));
    CHECK(dedup.insertIfNew(*p4));
    CHECK_EQ(dedup.size(), static_cast<size_t>(1));
}
