// FINDINGS.md section 68: bridge between the C++ library and the Python SMT
// sketch solver (scripts/sketch_solve.py).
//
//   export <gcd|factorial> <seed>            -> JSON on stdout: atoms, AST with
//                                              holes, training and held-out instances
//   verify <gcd|factorial> <seed> <fills>    -> rebuilds the same draw, fills the
//                                              holes from <fills>, checks with the real
//                                              interpreter and isSolution
//
// The draw is section 66's: ALGOSEARCH_FORCE_SKELETON + --skeleton-holes,
// Random(seed), generateRandomProgram(rng, 2, cfg). Holes are the sites in
// the hole grammar (search/HoleGrammar.hpp); atoms are ordered as there:
// scalars, literals 0..2, arr[scalar], arr[0..2].
//
// <fills> has one line per hole, in site order:
//   value <atom|+|-|*|/|%> A B
//   cond  <atom|+|-> A B <op> C          (op one of < > == <= >= !=)
//
// Build (after `make`):
//   g++ -std=c++20 -O2 -Isrc scripts/sketch_bridge.cpp \
//       $(find build/lib -name '*.o' ! -name main.o) -o sketch_bridge -lpthread

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include "interpreter/Interpreter.hpp"
#include "problems/FactorialProblem.hpp"
#include "problems/GcdProblem.hpp"
#include "search/HoleGrammar.hpp"
#include "search/ProgramGenerator.hpp"

using namespace algosearch;

namespace {

struct Setup {
    std::unique_ptr<Problem> problem;
    Program program;
    std::vector<int> vars;
    std::vector<Instance> train, heldOut;
};

Setup draw(const std::string& which, uint64_t seed) {
    Setup s;
    std::string skeleton;
    if (which == "gcd") {
        s.problem = std::make_unique<GcdProblem>(20, 40, 8);
        skeleton = "recurrence";
    } else {
        s.problem = std::make_unique<FactorialProblem>(15, 40, 8);
        skeleton = "repeatedmultiply";
    }
    int level = 1;
    for (; level <= 6; level = s.problem->nextLevel(level)) {
        for (auto& i : s.problem->generateInstances(level)) s.train.push_back(i);
    }
    for (int k = 0; k < 3; ++k, level = s.problem->nextLevel(level)) {
        for (auto& i : s.problem->generateInstances(level)) s.heldOut.push_back(i);
        for (auto& i : s.problem->generateAdversarialInstances(level)) s.heldOut.push_back(i);
    }
    setenv("ALGOSEARCH_FORCE_SKELETON", skeleton.c_str(), 1);
    GenConfig cfg;
    cfg.numVars = 8;
    cfg.loopAccumulatorBias = 1.0;
    cfg.combinatorBias = 0.0;
    cfg.skeletonHoles = true;
    Random rng(seed);
    s.program = generateRandomProgram(rng, 2, cfg);
    std::set<int> vs = {0};
    collectVars(*s.program, vs);
    s.vars.assign(vs.begin(), vs.end());
    return s;
}

std::vector<ExprPtr> atoms(const std::vector<int>& vars) {
    std::vector<ExprPtr> a;
    for (int v : vars) a.push_back(makeVar(v));
    for (int c = 0; c <= 2; ++c) a.push_back(makeIntConst(c));
    for (int v : vars) a.push_back(makeArrayAccess(makeVar(v)));
    for (int c = 0; c <= 2; ++c) a.push_back(makeArrayAccess(makeIntConst(c)));
    return a;
}

bool isHole(const Expr& e, bool isCond) { return isCond ? inCondGrammar(e) : inValueGrammar(e); }

const char* opName(ExprKind k) {
    switch (k) {
        case ExprKind::Add: return "+";
        case ExprKind::Sub: return "-";
        case ExprKind::Mul: return "*";
        case ExprKind::Div: return "/";
        case ExprKind::Mod: return "%";
        case ExprKind::Lt: return "<";
        case ExprKind::Gt: return ">";
        case ExprKind::Eq: return "==";
        case ExprKind::Le: return "<=";
        case ExprKind::Ge: return ">=";
        case ExprKind::Ne: return "!=";
        default: return "?";
    }
}

std::string exprJson(const Expr& e) {
    switch (e.kind) {
        case ExprKind::IntConst: return "{\"k\":\"const\",\"v\":" + std::to_string(e.intValue) + "}";
        case ExprKind::Var: return "{\"k\":\"var\",\"i\":" + std::to_string(e.varIndex) + "}";
        case ExprKind::ArrayAccess: return "{\"k\":\"arr\",\"i\":" + exprJson(*e.lhs) + "}";
        default:
            if (std::string(opName(e.kind)) == "?") {
                std::cerr << "unsupported expression " << e.toString() << "\n";
                std::exit(2);
            }
            return std::string("{\"k\":\"op\",\"op\":\"") + opName(e.kind) + "\",\"l\":" + exprJson(*e.lhs) + ",\"r\":" +
                   exprJson(*e.rhs) + "}";
    }
}

std::string slotJson(const Expr& e, bool isCond, int& holeId) {
    if (isHole(e, isCond)) return "{\"k\":\"hole\",\"t\":\"" + std::string(isCond ? "cond" : "value") + "\",\"id\":" +
                                  std::to_string(holeId++) + "}";
    return exprJson(e);
}

std::string stmtsJson(const std::vector<StmtPtr>& body, int& holeId);

std::string stmtJson(const Stmt& s, int& holeId) {
    switch (s.kind) {
        case StmtKind::Assign:
            return "{\"k\":\"assign\",\"i\":" + std::to_string(s.varIndex) + ",\"v\":" + slotJson(*s.valueExpr, false, holeId) + "}";
        case StmtKind::AssignArray: {
            std::string idx = exprJson(*s.indexExpr);
            return "{\"k\":\"assignArr\",\"i\":" + idx + ",\"v\":" + slotJson(*s.valueExpr, false, holeId) + "}";
        }
        case StmtKind::If: {
            std::string c = slotJson(*s.cond, true, holeId);
            std::string b = stmtsJson(s.body, holeId);
            return "{\"k\":\"if\",\"c\":" + c + ",\"b\":" + b + ",\"e\":" + stmtsJson(s.elseBody, holeId) + "}";
        }
        case StmtKind::While: {
            std::string c = slotJson(*s.cond, true, holeId);
            return "{\"k\":\"while\",\"c\":" + c + ",\"b\":" + stmtsJson(s.body, holeId) + "}";
        }
        case StmtKind::Block: return "{\"k\":\"block\",\"b\":" + stmtsJson(s.body, holeId) + "}";
        default:
            std::cerr << "unsupported statement\n" << s.toString() << "\n";
            std::exit(2);
    }
}

std::string stmtsJson(const std::vector<StmtPtr>& body, int& holeId) {
    std::string out = "[";
    for (size_t i = 0; i < body.size(); ++i) out += (i ? "," : "") + stmtJson(*body[i], holeId);
    return out + "]";
}

std::string instancesJson(const std::vector<Instance>& insts) {
    std::string out = "[";
    for (size_t i = 0; i < insts.size(); ++i) {
        out += i ? "," : "";
        out += "{\"in\":[";
        for (size_t j = 0; j < insts[i].input.size(); ++j) out += (j ? "," : "") + std::to_string(insts[i].input[j]);
        out += "],\"out\":" + std::to_string(insts[i].expectedOutput.empty() ? 0 : insts[i].expectedOutput[0]) + "}";
    }
    return out + "]";
}

ExprKind opKind(const std::string& op) {
    static const std::pair<const char*, ExprKind> table[] = {
        {"+", ExprKind::Add}, {"-", ExprKind::Sub}, {"*", ExprKind::Mul}, {"/", ExprKind::Div}, {"%", ExprKind::Mod},
        {"<", ExprKind::Lt},  {">", ExprKind::Gt},  {"==", ExprKind::Eq}, {"<=", ExprKind::Le}, {">=", ExprKind::Ge},
        {"!=", ExprKind::Ne}};
    for (const auto& [name, kind] : table) {
        if (op == name) return kind;
    }
    std::cerr << "bad op " << op << "\n";
    std::exit(2);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: sketch_bridge export|verify gcd|factorial seed [fills]\n";
        return 1;
    }
    std::string mode = argv[1];
    Setup s = draw(argv[2], std::stoull(argv[3]));
    if (mode == "export") {
        int holeId = 0;
        std::string program = stmtsJson(s.program->body, holeId);
        std::cout << "{\"vars\":[";
        for (size_t i = 0; i < s.vars.size(); ++i) std::cout << (i ? "," : "") << s.vars[i];
        std::cout << "],\"numVars\":8,\"holes\":" << holeId << ",\"program\":" << program << ",\"train\":" << instancesJson(s.train)
                  << ",\"heldOut\":" << instancesJson(s.heldOut) << "}\n";
        return 0;
    }
    // verify
    std::ifstream in(argv[4]);
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) {
        if (!line.empty()) lines.push_back(line);
    }
    std::vector<ExprPtr> at = atoms(s.vars);
    size_t h = 0;
    bool bad = false;
    forEachSlot(*s.program, [&](Stmt&, ExprPtr& slot, bool isCond) {
        if (!isHole(*slot, isCond)) return;
        if (h >= lines.size()) {
            bad = true;
            return;
        }
        std::istringstream ls(lines[h++]);
        std::string kind, form;
        int a = 0, b = 0;
        ls >> kind >> form >> a >> b;
        ExprPtr lhs = form == "atom" ? at[static_cast<size_t>(a)]->clone()
                                     : makeBinOp(opKind(form), at[static_cast<size_t>(a)]->clone(), at[static_cast<size_t>(b)]->clone());
        if (kind == "cond") {
            std::string op;
            int c = 0;
            ls >> op >> c;
            slot = makeCompare(opKind(op), std::move(lhs), at[static_cast<size_t>(c)]->clone());
        } else {
            slot = std::move(lhs);
        }
    });
    if (bad || h != lines.size()) {
        std::cout << "VERIFY bad fills (" << h << " holes, " << lines.size() << " lines)\n";
        return 1;
    }
    Interpreter interp(5'000'000);
    auto count = [&](const std::vector<Instance>& insts) {
        int ok = 0;
        std::vector<int> out;
        for (const auto& i : insts) ok += s.problem->evaluateInto(*s.program, i, interp, out) == ExecStatus::SUCCESS;
        return ok;
    };
    int t = count(s.train), ho = count(s.heldOut);
    std::cout << "VERIFY train " << t << "/" << s.train.size() << " heldout " << ho << "/" << s.heldOut.size()
              << ((t == static_cast<int>(s.train.size()) && ho == static_cast<int>(s.heldOut.size())) ? " CORRECT" : " WRONG") << "\n"
              << s.program->toString() << "\n";
    return 0;
}
