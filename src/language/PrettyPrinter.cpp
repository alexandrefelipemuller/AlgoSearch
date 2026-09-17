#include "language/PrettyPrinter.hpp"

#include <sstream>

#include "ast/Expr.hpp"
#include "ast/Stmt.hpp"

namespace algosearch {

namespace {

std::string varName(int idx) { return "v" + std::to_string(idx); }

const char* opSymbol(ExprKind k) {
    switch (k) {
        case ExprKind::Add: return "+";
        case ExprKind::Sub: return "-";
        case ExprKind::Lt: return "<";
        case ExprKind::Gt: return ">";
        case ExprKind::Eq: return "==";
        case ExprKind::Le: return "<=";
        case ExprKind::Ge: return ">=";
        case ExprKind::Ne: return "!=";
        case ExprKind::Mul: return "*";
        case ExprKind::Div: return "/";
        case ExprKind::Mod: return "%";
        case ExprKind::BitAnd: return "&";
        case ExprKind::BitOr: return "|";
        case ExprKind::BitXor: return "^";
        case ExprKind::Shl: return "<<";
        case ExprKind::Shr: return ">>";
        default: return "?";
    }
}

void indentLine(std::ostringstream& out, int indent) {
    for (int i = 0; i < indent; ++i) out << "  ";
}

void printStmtList(std::ostringstream& out, const std::vector<std::unique_ptr<Stmt>>& stmts, int indent) {
    for (const auto& s : stmts) {
        out << printStmt(*s, indent);
    }
}

}  // namespace

std::string printExpr(const Expr& e) {
    switch (e.kind) {
        case ExprKind::IntConst:
            return std::to_string(e.intValue);
        case ExprKind::Var:
            return varName(e.varIndex);
        case ExprKind::ArrayAccess:
            return "arr[" + printExpr(*e.lhs) + "]";
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
            return "(" + printExpr(*e.lhs) + " " + opSymbol(e.kind) + " " + printExpr(*e.rhs) + ")";
    }
    return "?";
}

std::string printStmt(const Stmt& s, int indent) {
    std::ostringstream out;
    switch (s.kind) {
        case StmtKind::Assign:
            indentLine(out, indent);
            out << varName(s.varIndex) << " = " << printExpr(*s.valueExpr) << ";\n";
            break;
        case StmtKind::AssignArray:
            indentLine(out, indent);
            out << "arr[" << printExpr(*s.indexExpr) << "] = " << printExpr(*s.valueExpr) << ";\n";
            break;
        case StmtKind::If:
            indentLine(out, indent);
            out << "if (" << printExpr(*s.cond) << ") {\n";
            printStmtList(out, s.body, indent + 1);
            indentLine(out, indent);
            out << "}";
            if (!s.elseBody.empty()) {
                out << " else {\n";
                printStmtList(out, s.elseBody, indent + 1);
                indentLine(out, indent);
                out << "}";
            }
            out << "\n";
            break;
        case StmtKind::While:
            indentLine(out, indent);
            out << "while (" << printExpr(*s.cond) << ") {\n";
            printStmtList(out, s.body, indent + 1);
            indentLine(out, indent);
            out << "}\n";
            break;
        case StmtKind::Swap:
            indentLine(out, indent);
            out << "swap(arr[" << printExpr(*s.indexExpr) << "], arr[" << printExpr(*s.indexExpr2) << "]);\n";
            break;
        case StmtKind::Return:
            indentLine(out, indent);
            out << "return;\n";
            break;
        case StmtKind::Block:
            printStmtList(out, s.body, indent);
            break;
    }
    return out.str();
}

}  // namespace algosearch
