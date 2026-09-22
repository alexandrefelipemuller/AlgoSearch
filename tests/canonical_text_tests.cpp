#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// Selection sort, two independent hand-written builds that differ ONLY in
// which scratch-variable number plays each role (outer index / inner index
// / min-so-far index) -- exactly the kind of cosmetic difference a mutated
// re-discovery of the same champion produces (FINDINGS.md section 29).
Program buildSelectionSort(int outerVar, int innerVar, int minVar) {
    auto root = makeBlock();
    root->body.push_back(makeAssign(outerVar, makeIntConst(0)));

    std::vector<StmtPtr> innerBody;
    std::vector<StmtPtr> ifBody;
    ifBody.push_back(makeAssign(minVar, makeVar(innerVar)));
    innerBody.push_back(
        makeIf(makeCompare(ExprKind::Lt, makeArrayAccess(makeVar(innerVar)), makeArrayAccess(makeVar(minVar))),
              std::move(ifBody)));
    innerBody.push_back(makeAssign(innerVar, makeBinOp(ExprKind::Add, makeVar(innerVar), makeIntConst(1))));

    std::vector<StmtPtr> outerBody;
    outerBody.push_back(makeAssign(minVar, makeVar(outerVar)));
    outerBody.push_back(makeAssign(innerVar, makeBinOp(ExprKind::Add, makeVar(outerVar), makeIntConst(1))));
    outerBody.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(innerVar), makeVar(0)), std::move(innerBody)));
    outerBody.push_back(makeSwap(makeVar(outerVar), makeVar(minVar)));
    outerBody.push_back(makeAssign(outerVar, makeBinOp(ExprKind::Add, makeVar(outerVar), makeIntConst(1))));

    root->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(outerVar), makeVar(0)), std::move(outerBody)));
    return root;
}

}  // namespace

TEST(canonical_text_ignores_cosmetic_variable_renaming) {
    Program a = buildSelectionSort(/*outer=*/4, /*inner=*/5, /*min=*/1);
    Program b = buildSelectionSort(/*outer=*/7, /*inner=*/6, /*min=*/2);

    CHECK(a->toString() != b->toString());  // raw text genuinely differs
    CHECK_EQ(canonicalText(*a), canonicalText(*b));  // canonical text does not
}

TEST(canonical_text_still_distinguishes_structurally_different_programs) {
    Program selectionSort = buildSelectionSort(4, 5, 1);

    auto other = makeBlock();
    other->body.push_back(makeAssign(1, makeIntConst(0)));
    other->body.push_back(makeAssignArray(makeIntConst(0), makeVar(1)));

    CHECK(canonicalText(*selectionSort) != canonicalText(*other));
}

TEST(canonical_text_leaves_v0_untouched) {
    // v0 is the implicit n/array-length register, never an arbitrary
    // scratch choice -- canonicalization must not renumber it. v1 is kept
    // alive by the arr[0] read below so the dead-store pass doesn't strip
    // the whole thing before this can be checked.
    auto a = makeBlock();
    a->body.push_back(makeAssign(1, makeVar(0)));
    a->body.push_back(makeAssignArray(makeIntConst(0), makeVar(1)));
    std::string text = canonicalText(*a);
    CHECK(text.find("v0") != std::string::npos);
    CHECK(text.find("v1") != std::string::npos);
}

TEST(canonical_text_strips_dead_trailing_assigns) {
    // FINDINGS.md sections 29-30: the divergence window kept archiving the
    // SAME selection-sort core padded with a differently-sized tail of
    // dead stores (`v3 = v4; v3 = v4;`, `v2 = v7;`, ...) as "distinct"
    // solutions. A dead store's target is never read again anywhere.
    Program clean = buildSelectionSort(4, 5, 1);

    Program padded = buildSelectionSort(4, 5, 1);
    padded->body.push_back(makeAssign(7, makeVar(2)));  // v7 = v2; -- v7 read nowhere else
    padded->body.push_back(makeAssign(7, makeVar(2)));  // repeated, still dead

    CHECK(clean->toString() != padded->toString());  // raw text genuinely differs
    CHECK_EQ(canonicalText(*clean), canonicalText(*padded));  // canonical text does not
}

TEST(canonical_text_keeps_assigns_that_are_read_later) {
    // A store IS live if its target is read anywhere downstream -- even
    // through a later, otherwise-unrelated statement. Must not be stripped.
    auto a = makeBlock();
    a->body.push_back(makeAssign(3, makeIntConst(7)));         // v3 = 7;
    a->body.push_back(makeAssignArray(makeIntConst(0), makeVar(3)));  // arr[0] = v3; -- reads v3

    std::string text = canonicalText(*a);
    CHECK(text.find("= 7") != std::string::npos);
}

TEST(canonical_text_strips_a_redundant_final_write_to_an_otherwise_live_variable) {
    // FINDINGS.md section 30: the narrower "read anywhere in the whole
    // program" rule failed on exactly this shape -- a loop counter that IS
    // read many times inside the loop, plus one more write to it right
    // after the loop ends that nothing subsequently reads. Real observed
    // pair: a clean selection sort vs. the same selection sort + `v4 = 3;`
    // appended at the very end.
    Program clean = buildSelectionSort(4, 5, 1);

    Program padded = buildSelectionSort(4, 5, 1);
    padded->body.push_back(makeAssign(4, makeIntConst(3)));  // v4 = 3; -- v4 IS live earlier (the loop counter), just not after this

    CHECK(clean->toString() != padded->toString());
    CHECK_EQ(canonicalText(*clean), canonicalText(*padded));
}

TEST(canonical_text_keeps_a_write_whose_value_is_read_on_the_loops_next_iteration) {
    // Loop-carried liveness: a variable written near the end of a loop body
    // and read near the top of the SAME body on the next iteration must
    // NOT be stripped, even though textually the read appears "before" the
    // write within one pass -- the fixed-point analysis must see the
    // back-edge. `v2` here plays exactly that role in selection sort's
    // outer loop (min-index carried from the inner scan into the swap).
    Program sel = buildSelectionSort(/*outer=*/4, /*inner=*/5, /*min=*/2);
    std::string text = canonicalText(*sel);
    CHECK(text.find("swap(") != std::string::npos);
    // The whole loop body must survive intact -- nothing in it is dead.
    CHECK(text.find("while (") != std::string::npos);
}

TEST(canonical_text_folds_a_constant_false_if_guard) {
    // Real observed pattern (FINDINGS.md section 30's remaining gap):
    // `if (0 >= 3) { ...uses several vars... }` appended after a clean
    // selection sort. The guard is always false regardless of any
    // variable's value, so the whole branch -- and, once it's gone, the
    // vars only read inside it -- must vanish from the canonical form.
    Program clean = buildSelectionSort(4, 5, 1);

    Program padded = buildSelectionSort(4, 5, 1);
    std::vector<StmtPtr> deadBranch;
    deadBranch.push_back(makeAssign(7, makeBinOp(ExprKind::Add, makeVar(2), makeIntConst(1))));
    padded->body.push_back(makeIf(makeCompare(ExprKind::Ge, makeIntConst(0), makeIntConst(3)), std::move(deadBranch)));

    CHECK(clean->toString() != padded->toString());
    CHECK_EQ(canonicalText(*clean), canonicalText(*padded));
}

TEST(canonical_text_folds_a_self_compared_while_condition) {
    // `while (v6 < v6) { ... }` can never be true for any value of v6 --
    // zero iterations, always, regardless of what v6 holds.
    Program clean = buildSelectionSort(4, 5, 1);

    Program padded = buildSelectionSort(4, 5, 1);
    std::vector<StmtPtr> deadLoopBody;
    deadLoopBody.push_back(makeAssign(3, makeBinOp(ExprKind::Add, makeVar(1), makeIntConst(2))));
    padded->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(6), makeVar(6)), std::move(deadLoopBody)));

    CHECK(clean->toString() != padded->toString());
    CHECK_EQ(canonicalText(*clean), canonicalText(*padded));
}

TEST(canonical_text_does_not_fold_a_data_dependent_condition) {
    // `if (v7 <= v4) {...}` is NOT decidable from text alone -- it depends
    // on what v7/v4 actually hold at runtime. Must be left exactly as
    // written (this is the harder case explicitly out of scope: real
    // constant propagation, not condition folding).
    auto a = makeBlock();
    std::vector<StmtPtr> body;
    body.push_back(makeAssign(3, makeVar(2)));
    a->body.push_back(makeIf(makeCompare(ExprKind::Le, makeVar(7), makeVar(4)), std::move(body)));

    std::string text = canonicalText(*a);
    CHECK(text.find("if (") != std::string::npos);
}

TEST(canonical_text_keeps_assigns_read_only_inside_a_loop_body) {
    // The read can be nested arbitrarily deep (inside a while/if) -- the
    // read-collection pass must see it regardless of nesting.
    std::vector<StmtPtr> whileBody;
    whileBody.push_back(makeAssignArray(makeIntConst(0), makeVar(2)));  // arr[0] = v2; -- reads v2
    auto a = makeBlock();
    a->body.push_back(makeAssign(2, makeIntConst(9)));  // v2 = 9;
    a->body.push_back(makeWhile(makeCompare(ExprKind::Lt, makeVar(0), makeIntConst(1)), std::move(whileBody)));

    std::string text = canonicalText(*a);
    CHECK(text.find("= 9") != std::string::npos);
}
