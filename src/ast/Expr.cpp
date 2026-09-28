#include "ast/Expr.hpp"

#include "language/PrettyPrinter.hpp"

namespace algosearch {

std::unique_ptr<Expr> Expr::clone() const {
    auto n = std::make_unique<Expr>();
    n->kind = kind;
    n->intValue = intValue;
    n->varIndex = varIndex;
    if (lhs) n->lhs = lhs->clone();
    if (rhs) n->rhs = rhs->clone();
    return n;
}

int Expr::size() const {
    int total = 1;
    if (lhs) total += lhs->size();
    if (rhs) total += rhs->size();
    return total;
}

std::string Expr::toString() const { return printExpr(*this); }

bool isBinaryKind(ExprKind kind) {
    switch (kind) {
        case ExprKind::Add:
        case ExprKind::Sub:
        case ExprKind::Lt:
        case ExprKind::Gt:
        case ExprKind::Eq:
        case ExprKind::Le:
        case ExprKind::Ge:
        case ExprKind::Ne:
        case ExprKind::Mul:
        case ExprKind::Div:
        case ExprKind::Mod:
        case ExprKind::BitAnd:
        case ExprKind::BitOr:
        case ExprKind::BitXor:
        case ExprKind::Shl:
        case ExprKind::Shr:
            return true;
        default:
            return false;
    }
}

ExprPtr makeIntConst(int value) {
    auto n = std::make_unique<Expr>();
    n->kind = ExprKind::IntConst;
    n->intValue = value;
    return n;
}

ExprPtr makeVar(int varIndex) {
    auto n = std::make_unique<Expr>();
    n->kind = ExprKind::Var;
    n->varIndex = varIndex;
    return n;
}

ExprPtr makeArrayAccess(ExprPtr index) {
    auto n = std::make_unique<Expr>();
    n->kind = ExprKind::ArrayAccess;
    n->lhs = std::move(index);
    return n;
}

ExprPtr makeAuxAccess(ExprPtr index) {
    auto n = std::make_unique<Expr>();
    n->kind = ExprKind::AuxAccess;
    n->lhs = std::move(index);
    return n;
}

ExprPtr makeBinOp(ExprKind kind, ExprPtr lhs, ExprPtr rhs) {
    auto n = std::make_unique<Expr>();
    n->kind = kind;
    n->lhs = std::move(lhs);
    n->rhs = std::move(rhs);
    return n;
}

ExprPtr makeCompare(ExprKind kind, ExprPtr lhs, ExprPtr rhs) { return makeBinOp(kind, std::move(lhs), std::move(rhs)); }

bool exprEqual(const Expr* a, const Expr* b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return false;
    if (a->kind == ExprKind::IntConst) return a->intValue == b->intValue;
    if (a->kind == ExprKind::Var) return a->varIndex == b->varIndex;
    return exprEqual(a->lhs.get(), b->lhs.get()) && exprEqual(a->rhs.get(), b->rhs.get());
}

std::optional<bool> constantConditionValue(const Expr& cond) {
    bool isComparison = cond.kind == ExprKind::Lt || cond.kind == ExprKind::Gt || cond.kind == ExprKind::Eq ||
                        cond.kind == ExprKind::Le || cond.kind == ExprKind::Ge || cond.kind == ExprKind::Ne;
    if (!isComparison || !cond.lhs || !cond.rhs) return std::nullopt;

    auto evalWith = [&](int a, int b) -> bool {
        switch (cond.kind) {
            case ExprKind::Lt: return a < b;
            case ExprKind::Gt: return a > b;
            case ExprKind::Eq: return a == b;
            case ExprKind::Le: return a <= b;
            case ExprKind::Ge: return a >= b;
            case ExprKind::Ne: return a != b;
            default: return false;  // unreachable given isComparison above
        }
    };

    if (cond.lhs->kind == ExprKind::IntConst && cond.rhs->kind == ExprKind::IntConst) {
        return evalWith(cond.lhs->intValue, cond.rhs->intValue);
    }
    if (exprEqual(cond.lhs.get(), cond.rhs.get())) {
        return evalWith(0, 0);  // any x compared with itself behaves like x==x
    }
    return std::nullopt;
}

}  // namespace algosearch
