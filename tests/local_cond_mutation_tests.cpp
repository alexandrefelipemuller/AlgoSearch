#include <string>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"
#include "core/Random.hpp"
#include "search/ProgramGenerator.hpp"
#include "test_framework.hpp"

using namespace algosearch;

namespace {

// if (v3 <= (v0 / 2)) v4 = -1;  -- the section 113 sentinel guard shape.
StmtPtr guardedProgram() {
    auto root = makeBlock();
    std::vector<StmtPtr> then;
    then.push_back(makeAssign(4, makeIntConst(-1)));
    root->body.push_back(makeIf(makeCompare(ExprKind::Le, makeVar(3), makeBinOp(ExprKind::Div, makeVar(0), makeIntConst(2))),
                                std::move(then)));
    return root;
}

}  // namespace

TEST(local_cond_mutation_is_byte_identical_when_off) {
    GenConfig cfg;
    cfg.numVars = 6;
    StmtPtr seed = guardedProgram();
    Random ra(115), rb(115);
    GenConfig off = cfg;
    off.localCondMutationRate = 0.0;
    for (int i = 0; i < 200; ++i) {
        CHECK_EQ(growProgram(*seed, 8, ra, cfg)->toString(), growProgram(*seed, 8, rb, off)->toString());
    }
}

TEST(local_cond_mutation_reaches_neighbouring_guards) {
    // With only the local step acting (no growth, no other mutation is
    // guaranteed), the guard's neighbours appear: another comparator, and
    // the threshold n / 2 unwrapped to n.
    GenConfig cfg;
    cfg.numVars = 6;
    cfg.localCondMutationRate = 1.0;
    StmtPtr seed = guardedProgram();
    std::string original = seed->toString();
    Random rng(116);
    bool comparator = false, unwrapped = false;
    for (int i = 0; i < 2000; ++i) {
        std::string text = growProgram(*seed, seed->size(), rng, cfg)->toString();
        comparator = comparator || text.find("(v3 < (v0 / 2))") != std::string::npos;
        unwrapped = unwrapped || text.find("(v3 <= v0)") != std::string::npos;
    }
    CHECK(comparator);
    CHECK(unwrapped);
}
