#include "ast/Stmt.hpp"

#include "language/PrettyPrinter.hpp"

namespace algosearch {

std::unique_ptr<Stmt> Stmt::clone() const {
    auto n = std::make_unique<Stmt>();
    n->kind = kind;
    n->varIndex = varIndex;
    if (valueExpr) n->valueExpr = valueExpr->clone();
    if (indexExpr) n->indexExpr = indexExpr->clone();
    if (indexExpr2) n->indexExpr2 = indexExpr2->clone();
    if (cond) n->cond = cond->clone();
    n->body.reserve(body.size());
    for (const auto& s : body) n->body.push_back(s->clone());
    n->elseBody.reserve(elseBody.size());
    for (const auto& s : elseBody) n->elseBody.push_back(s->clone());
    return n;
}

int Stmt::size() const {
    int total = 1;
    if (valueExpr) total += valueExpr->size();
    if (indexExpr) total += indexExpr->size();
    if (indexExpr2) total += indexExpr2->size();
    if (cond) total += cond->size();
    for (const auto& s : body) total += s->size();
    for (const auto& s : elseBody) total += s->size();
    return total;
}

std::string Stmt::toString(int indent) const { return printStmt(*this, indent); }

StmtPtr makeBlock() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Block;
    return n;
}

StmtPtr makeAssign(int varIndex, ExprPtr value) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Assign;
    n->varIndex = varIndex;
    n->valueExpr = std::move(value);
    return n;
}

StmtPtr makeAssignArray(ExprPtr index, ExprPtr value) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::AssignArray;
    n->indexExpr = std::move(index);
    n->valueExpr = std::move(value);
    return n;
}

StmtPtr makeIf(ExprPtr cond, std::vector<StmtPtr> body, std::vector<StmtPtr> elseBody) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::If;
    n->cond = std::move(cond);
    n->body = std::move(body);
    n->elseBody = std::move(elseBody);
    return n;
}

StmtPtr makeWhile(ExprPtr cond, std::vector<StmtPtr> body) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::While;
    n->cond = std::move(cond);
    n->body = std::move(body);
    return n;
}

StmtPtr makeSwap(ExprPtr index1, ExprPtr index2) {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Swap;
    n->indexExpr = std::move(index1);
    n->indexExpr2 = std::move(index2);
    return n;
}

StmtPtr makeReturn() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Return;
    return n;
}

StmtPtr makeContinue() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Continue;
    return n;
}

StmtPtr makeBreak() {
    auto n = std::make_unique<Stmt>();
    n->kind = StmtKind::Break;
    return n;
}

}  // namespace algosearch
