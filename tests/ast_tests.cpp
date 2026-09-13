#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "test_framework.hpp"

using namespace algosearch;

TEST(expr_size_and_clone) {
    // (v0 + 3)
    auto e = makeBinOp(ExprKind::Add, makeVar(0), makeIntConst(3));
    CHECK_EQ(e->size(), 3);  // Add, Var, IntConst

    auto cloned = e->clone();
    CHECK_EQ(cloned->size(), e->size());
    CHECK_EQ(cloned->toString(), e->toString());

    // Mutating the clone must not affect the original (deep clone).
    cloned->intValue = 999;  // no-op for Add kind, but lhs/rhs must be independent pointers
    CHECK(cloned->lhs.get() != e->lhs.get());
    CHECK(cloned->rhs.get() != e->rhs.get());
}

TEST(array_access_pretty_print) {
    auto access = makeArrayAccess(makeVar(1));
    CHECK_EQ(access->toString(), std::string("arr[v1]"));
}

TEST(stmt_size_counts_all_nodes) {
    auto block = makeBlock();
    block->body.push_back(makeAssign(0, makeIntConst(5)));
    block->body.push_back(makeSwap(makeIntConst(0), makeIntConst(1)));
    // Assign: 1(stmt) + 1(IntConst) = 2
    // Swap:   1(stmt) + 1 + 1 = 3
    // Block:  1 + 2 + 3 = 6
    CHECK_EQ(block->size(), 6);
}

TEST(stmt_clone_is_deep) {
    auto block = makeBlock();
    block->body.push_back(makeAssign(0, makeIntConst(1)));
    auto clone = block->clone();
    clone->body[0]->varIndex = 3;
    CHECK_EQ(block->body[0]->varIndex, 0);
    CHECK_EQ(clone->body[0]->varIndex, 3);
}

TEST(if_with_else_pretty_print_roundtrip) {
    std::vector<StmtPtr> thenBody;
    thenBody.push_back(makeAssign(0, makeIntConst(1)));
    std::vector<StmtPtr> elseBody;
    elseBody.push_back(makeAssign(0, makeIntConst(2)));
    auto ifStmt = makeIf(makeCompare(ExprKind::Lt, makeVar(0), makeIntConst(5)), std::move(thenBody), std::move(elseBody));
    std::string text = ifStmt->toString();
    CHECK(text.find("if (") != std::string::npos);
    CHECK(text.find("else") != std::string::npos);
}
