// Sema.cpp
#include "Sema.h"
#include <iostream>

// =====================================================
// unwrap identifier from common wrappers (Identifier / LValue / Factor / Expr / CondExpr)
// =====================================================

static ASTNodePtr unwrapIdent(const ASTNodePtr& n) {
    if (!n) return nullptr;

    if (n->type == ASTNodeType::Identifier) return n;

    if (n->type == ASTNodeType::LValue && !n->children.empty())
        return unwrapIdent(n->children[0]);

    if ((n->type == ASTNodeType::Factor ||
        n->type == ASTNodeType::Expr ||
        n->type == ASTNodeType::CondExpr) &&
        !n->children.empty())
        return unwrapIdent(n->children[0]);


    return nullptr;
}

static bool isNum(ValueType t) {
    return t == ValueType::TYPE_INT || t == ValueType::TYPE_FLOAT;
}

static ValueType promote(ValueType a, ValueType b) {
    if (a == ValueType::TYPE_FLOAT || b == ValueType::TYPE_FLOAT) return ValueType::TYPE_FLOAT;
    if (a == ValueType::TYPE_INT && b == ValueType::TYPE_INT) return ValueType::TYPE_INT;
    return ValueType::TYPE_UNKNOWN;
}

// =====================================================
// ctor / public
// =====================================================

Sema::Sema(const ASTNodePtr& root)
    : root(root) {}

bool Sema::analyze() {
    visit(root);
    return !hasError;
}

// =====================================================
// error reporting
// =====================================================

void Sema::report(const Token& tok, const std::string& msg) {
    std::cerr << "Semantic error at line " << tok.line
              << ", col " << tok.column << ": " << msg << "\n";
    hasError = true;
}

// =====================================================
// symbol helpers
// =====================================================

bool Sema::isVarDefined(const std::string& name, const Token& tok) {
    if (symbols.count(name) == 0) {
        report(tok, "Variable '" + name + "' used before definition");
        return false;
    }
    return true;
}

ValueType Sema::stringToType(const std::string& s) {
    if (s == "int")   return ValueType::TYPE_INT;
    if (s == "float") return ValueType::TYPE_FLOAT;
    if (s == "bool")  return ValueType::TYPE_BOOL;
    return ValueType::TYPE_UNKNOWN;
}

// =====================================================
// dispatcher
// =====================================================

void Sema::visit(const ASTNodePtr& node) {
    if (!node) return;

    switch (node->type) {
        case ASTNodeType::Program:
        case ASTNodeType::StmtList:
            visitStmtList(node);
            break;

        case ASTNodeType::VarDecl:
            visitVarDecl(node);
            break;

        case ASTNodeType::ArrayDecl:
            visitArrayDecl(node);
            break;

        case ASTNodeType::AssignOpStmt:
            visitAssignOpStmt(node);
            break;

        case ASTNodeType::PrintStmt:
            visitPrint(node);
            break;

        case ASTNodeType::IfStmt:
            visitIf(node);
            break;

        case ASTNodeType::WhileStmt:
            visitWhile(node);
            break;

        case ASTNodeType::ForStmt:
            visitFor(node);
            break;

        case ASTNodeType::ForeachStmt:
            visitForeach(node);
            break;

        case ASTNodeType::Block:
            visitBlock(node);
            break;

        case ASTNodeType::ForInit:
        case ASTNodeType::ForUpdate:
        case ASTNodeType::ForCond:
            for (auto& ch : node->children) visit(ch);
            break;

        default:
            break;
    }
}

// =====================================================
// statements
// =====================================================

void Sema::visitStmtList(const ASTNodePtr& node) {
    for (auto& ch : node->children) visit(ch);
}

void Sema::visitVarDecl(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    auto list = node->children[0];

    for (auto& item : list->children) {
        if (!item || item->children.size() < 2) continue;

        auto idNode = item->children[0];
        auto typeNode = item->children[1];

        std::string name = idNode->token.lexeme;
        std::string typeStr = typeNode->token.lexeme;

        if (symbols.count(name)) {
            report(idNode->token, "Redefinition of variable '" + name + "'");
            continue;
        }

        symbols[name] = stringToType(typeStr);

        if (item->children.size() >= 3) {
            ValueType rhsType = checkExpr(item->children[2]);
            if (rhsType != ValueType::TYPE_UNKNOWN && symbols[name] != rhsType) {
                report(idNode->token, "Type mismatch in initialization of '" + name + "'");
            }
        }
    }
}

void Sema::visitArrayDecl(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    auto idNode = node->children[0];
    std::string name = idNode->token.lexeme;

    if (symbols.count(name))
        report(idNode->token, "Redefinition of array '" + name + "'");

    symbols[name] = ValueType::TYPE_ARRAY;

    // optionally validate initializer
    if (node->children.size() >= 2) checkExpr(node->children[1]);
}

void Sema::visitAssignOpStmt(const ASTNodePtr& node) {
    if (!node) return;
    TokenType op = node->token.type;

    auto getName = [&](int i, const char* what) -> std::string {
        if (i < 0 || i >= (int)node->children.size()) {
            report(node->token, std::string("Malformed ") + what);
            return "";
        }
        auto id = unwrapIdent(node->children[(size_t)i]);
        if (!id) {
            report(node->token, std::string("Malformed ") + what);
            return "";
        }
        return id->token.lexeme;
    };

    std::string x = getName(0, "assignment op target");
    if (x.empty()) return;

    if (!isVarDefined(x, node->children[0]->token)) return;
    ValueType tx = symbols[x];

    if (op == TokenType::KW_INC || op == TokenType::KW_DEC) {
        if (tx != ValueType::TYPE_INT)
            report(node->token, "INC/DEC only allowed on int variables");
        return;
    }

    std::string y = getName(1, "assignment op operand");
    if (y.empty()) return;
    if (!isVarDefined(y, node->children[1]->token)) return;
    ValueType ty = symbols[y];

    if (op == TokenType::KW_PLE || op == TokenType::KW_MIE) {
        if (!(isNum(tx) && isNum(ty)))
            report(node->token, "PLE/MIE operands must be numeric");
        return;
    }

    std::string z = getName(2, "assignment op operand");
    if (z.empty()) return;
    if (!isVarDefined(z, node->children[2]->token)) return;
    ValueType tz = symbols[z];

    if (op == TokenType::KW_AND || op == TokenType::KW_OR) {
        if (tx != ValueType::TYPE_BOOL || ty != ValueType::TYPE_BOOL || tz != ValueType::TYPE_BOOL)
            report(node->token, "AND/OR only allowed on bool variables");
        return;
    }

    if (!(isNum(tx) && isNum(ty) && isNum(tz))) {
        report(node->token, "Arithmetic operands must be numeric");
        return;
    }

    ValueType res = promote(ty, tz);
    if (res != ValueType::TYPE_UNKNOWN && tx != res)
        report(node->token, "Type mismatch in assignment operation");
}

void Sema::visitPrint(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    checkExpr(node->children[0]);
}

void Sema::visitIf(const ASTNodePtr& node) {
    if (!node) return;

    // else-wrapper node: token is KW_ELSE and it only wraps one child (block or nested if)
    if (node->token.type == TokenType::KW_ELSE) {
        if (node->children.empty()) {
            report(node->token, "Malformed else clause");
            return;
        }
        visit(node->children[0]);
        return;
    }

    // normal if: children = [cond, thenBlock, (optional elsePart)]
    if (node->children.size() < 2) {
        report(node->token, "Malformed if statement");
        return;
    }

    ValueType condType = checkExpr(node->children[0]);
    if (condType != ValueType::TYPE_BOOL)
        report(node->children[0]->token, "If condition must be boolean");

    visit(node->children[1]);

    if (node->children.size() >= 3)
        visit(node->children[2]);
}


void Sema::visitWhile(const ASTNodePtr& node) {
    if (!node || node->children.size() < 2) return;

    ValueType condType = checkExpr(node->children[0]);
    if (condType != ValueType::TYPE_BOOL)
        report(node->children[0]->token, "While condition must be boolean");

    visit(node->children[1]);
}

void Sema::visitFor(const ASTNodePtr& node) {
    if (!node || node->children.size() < 4) return;

    visit(node->children[0]);

    if (!node->children[1] || node->children[1]->children.empty()) {
        report(node->token, "Malformed for condition");
    } else {
        ValueType condType = checkExpr(node->children[1]->children[0]);
        if (condType != ValueType::TYPE_BOOL)
            report(node->children[1]->token, "For condition must be boolean");
    }

    visit(node->children[2]);
    visit(node->children[3]);
}

void Sema::visitForeach(const ASTNodePtr& node) {
    if (!node || node->children.size() < 3) return;

    std::string varName = node->children[0]->token.lexeme;
    std::string arrName = node->children[1]->token.lexeme;

    symbols[varName] = ValueType::TYPE_INT;

    if (!isVarDefined(arrName, node->children[1]->token)) return;
    if (symbols[arrName] != ValueType::TYPE_ARRAY)
        report(node->children[1]->token, "Foreach target must be an array");

    visit(node->children[2]);
}

void Sema::visitBlock(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    visit(node->children[0]);
}

// =====================================================
// expression typing
// =====================================================

ValueType Sema::checkExpr(const ASTNodePtr& node) {
    if (!node) return ValueType::TYPE_UNKNOWN;

    switch (node->type) {
        case ASTNodeType::Expr:
        case ASTNodeType::CondExpr:
            if (node->children.empty()) return ValueType::TYPE_UNKNOWN;
            return checkExpr(node->children[0]);

        case ASTNodeType::BoolOr:
        case ASTNodeType::BoolAnd: {
            if (node->children.size() < 2) return ValueType::TYPE_UNKNOWN;
            ValueType L = checkExpr(node->children[0]);
            ValueType R = checkExpr(node->children[1]);
            if (L != ValueType::TYPE_BOOL || R != ValueType::TYPE_BOOL)
                report(node->token, "Logical operands must be bool");
            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::RelExpr: {
            if (node->children.size() < 2) return ValueType::TYPE_UNKNOWN;
            ValueType L = checkExpr(node->children[0]);
            ValueType R = checkExpr(node->children[1]);

            if (L == ValueType::TYPE_UNKNOWN || R == ValueType::TYPE_UNKNOWN)
                return ValueType::TYPE_BOOL;

            if (L != R && !(isNum(L) && isNum(R)))
                report(node->token, "Operands of comparison must be compatible");

            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::ArithExpr:
        case ASTNodeType::Term:
        case ASTNodeType::Power: {
            if (node->children.empty()) return ValueType::TYPE_UNKNOWN;
            ValueType L = checkExpr(node->children[0]);
            if (node->children.size() == 1) return L;

            if (node->children.size() < 2) return ValueType::TYPE_UNKNOWN;
            ValueType R = checkExpr(node->children[1]);

            if (!isNum(L) || !isNum(R))
                report(node->token, "Arithmetic operands must be numeric");

            return promote(L, R);
        }

        case ASTNodeType::Factor:
            if (node->children.empty()) return ValueType::TYPE_UNKNOWN;
            return checkExpr(node->children[0]);

        case ASTNodeType::LValue: {
            auto id = unwrapIdent(node);
            if (!id) return ValueType::TYPE_UNKNOWN;

            std::string name = id->token.lexeme;
            if (!isVarDefined(name, id->token)) return ValueType::TYPE_UNKNOWN;

            ValueType base = symbols[name];
            if (base == ValueType::TYPE_ARRAY) return ValueType::TYPE_INT; // element type assumption
            return base;
        }

        case ASTNodeType::Identifier: {
            std::string name = node->token.lexeme;
            if (!isVarDefined(name, node->token)) return ValueType::TYPE_UNKNOWN;
            return symbols[name];
        }

        case ASTNodeType::IntLiteral:   return ValueType::TYPE_INT;
        case ASTNodeType::FloatLiteral: return ValueType::TYPE_FLOAT;
        case ASTNodeType::BoolLiteral:  return ValueType::TYPE_BOOL;

        case ASTNodeType::BuiltinCall: {
            std::string fname = node->token.lexeme;

            if (fname == "to_int")   return ValueType::TYPE_INT;
            if (fname == "to_float") return ValueType::TYPE_FLOAT;
            if (fname == "to_bool")  return ValueType::TYPE_BOOL;

            if (fname == "length") {
                if (!node->children.empty()) checkExpr(node->children[0]);
                return ValueType::TYPE_INT;
            }

            if (fname == "abs") {
                if (!node->children.empty()) {
                    ValueType t = checkExpr(node->children[0]);
                    if (!isNum(t)) report(node->token, "abs expects numeric");
                    return t;
                }
                return ValueType::TYPE_UNKNOWN;
            }

            if (fname == "max" || fname == "index" || fname == "find") {
                if (!node->children.empty()) checkExpr(node->children[0]);
                return ValueType::TYPE_INT;
            }

            if (!node->children.empty()) checkExpr(node->children[0]);
            return ValueType::TYPE_UNKNOWN;
        }

        case ASTNodeType::ArgList:
            for (auto& ch : node->children) checkExpr(ch);
            return node->children.empty() ? ValueType::TYPE_UNKNOWN : checkExpr(node->children[0]);

        default:
            for (auto& ch : node->children) checkExpr(ch);
            return ValueType::TYPE_UNKNOWN;
    }
}
