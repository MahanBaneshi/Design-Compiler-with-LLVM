// Sema.cpp
#include "Sema.h"
#include <iostream>

Sema::Sema(const ASTNodePtr& root)
    : root(root) {}

bool Sema::analyze() {
    hasError = false;
    errors.clear();
    visit(root);
    return !hasError;
}

void Sema::report(const Token& tok, const std::string& msg) {
    errors.push_back(SemaError{ tok.line, tok.column, msg });
    hasError = true;
    std::cerr << "Semantic error at line " << tok.line
              << ", col " << tok.column << ": " << msg << "\n";
}

bool Sema::isVarDefined(const std::string& name, const Token& tok) {
    if (symbols.count(name) == 0) {
        report(tok, "Variable '" + name + "' used before definition");
        return false;
    }
    return true;
}

ValueType Sema::stringToType(const std::string& s) {
    if (s == "int") return ValueType::TYPE_INT;
    if (s == "float") return ValueType::TYPE_FLOAT;
    if (s == "bool") return ValueType::TYPE_BOOL;
    return ValueType::TYPE_UNKNOWN;
}

// =============================
// Dispatcher
// =============================
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

// =============================
// Visiting statements
// =============================
void Sema::visitStmtList(const ASTNodePtr& node) {
    for (auto& ch : node->children) visit(ch);
}

void Sema::visitVarDecl(const ASTNodePtr& node) {
    if (node->children.empty()) return;
    auto list = node->children[0];

    for (auto& item : list->children) {
        if (item->children.size() < 2) continue;

        auto idNode = item->children[0];
        auto typeNode = item->children[1];

        std::string name = idNode->token.lexeme;
        std::string typeStr = typeNode->token.lexeme;

        if (symbols.count(name)) {
            report(idNode->token, "Redefinition of variable '" + name + "'");
            continue;
        }

        ValueType vt = stringToType(typeStr);
        symbols[name] = vt;

        if (item->children.size() >= 3) {
            ValueType rhsType = checkExpr(item->children[2]);
            if (vt != ValueType::TYPE_UNKNOWN && rhsType != ValueType::TYPE_UNKNOWN && vt != rhsType) {
                report(idNode->token, "Type mismatch in initialization of '" + name + "'");
            }
        }
    }
}

void Sema::visitArrayDecl(const ASTNodePtr& node) {
    if (node->children.empty()) return;

    auto idNode = node->children[0];
    std::string name = idNode->token.lexeme;

    if (symbols.count(name)) {
        report(idNode->token, "Redefinition of array '" + name + "'");
        return;
    }

    symbols[name] = ValueType::TYPE_ARRAY;
}

void Sema::visitAssignOpStmt(const ASTNodePtr& node) {
    if (!node) return;
    TokenType op = node->token.type;

    auto needName = [&](int i, const char* what) -> std::string {
        if (i < 0 || i >= (int)node->children.size()) {
            report(node->token, std::string("Malformed ") + what);
            return "";
        }
        auto n = node->children[(size_t)i];
        if (!n) {
            report(node->token, std::string("Malformed ") + what);
            return "";
        }

        if (n->type == ASTNodeType::Identifier) return n->token.lexeme;

        if (n->type == ASTNodeType::Factor && !n->children.empty() &&
            n->children[0] && n->children[0]->type == ASTNodeType::Identifier) {
            return n->children[0]->token.lexeme;
        }

        report(node->token, std::string("Malformed ") + what);
        return "";
    };

    std::string x = needName(0, "assignment op target");
    if (x.empty()) return;
    if (!isVarDefined(x, node->children[0]->token)) return;

    ValueType tx = symbols[x];

    if (op == TokenType::KW_INC || op == TokenType::KW_DEC) {
        if (tx != ValueType::TYPE_INT) report(node->token, "INC/DEC only allowed on int variables");
        return;
    }

    std::string y = needName(1, "assignment op operand");
    if (y.empty()) return;
    if (!isVarDefined(y, node->children[1]->token)) return;
    ValueType ty = symbols[y];

    if (op == TokenType::KW_PLE || op == TokenType::KW_MIE) {
        bool xNum = (tx == ValueType::TYPE_INT || tx == ValueType::TYPE_FLOAT);
        bool yNum = (ty == ValueType::TYPE_INT || ty == ValueType::TYPE_FLOAT);
        if (!xNum || !yNum) report(node->token, "PLE/MIE operands must be numeric");
        return;
    }

    std::string z = needName(2, "assignment op operand");
    if (z.empty()) return;
    if (!isVarDefined(z, node->children[2]->token)) return;
    ValueType tz = symbols[z];

    if (op == TokenType::KW_AND || op == TokenType::KW_OR) {
        if (tx != ValueType::TYPE_BOOL || ty != ValueType::TYPE_BOOL || tz != ValueType::TYPE_BOOL) {
            report(node->token, "AND/OR only allowed on bool variables");
        }
        return;
    }

    bool xNum = (tx == ValueType::TYPE_INT || tx == ValueType::TYPE_FLOAT);
    bool yNum = (ty == ValueType::TYPE_INT || ty == ValueType::TYPE_FLOAT);
    bool zNum = (tz == ValueType::TYPE_INT || tz == ValueType::TYPE_FLOAT);
    if (!xNum) report(node->token, "Assignment target must be numeric for arithmetic operation");
    if (!yNum || !zNum) report(node->token, "Arithmetic operands must be numeric");

    ValueType result = (ty == ValueType::TYPE_FLOAT || tz == ValueType::TYPE_FLOAT)
                         ? ValueType::TYPE_FLOAT
                         : ValueType::TYPE_INT;

    if (tx != result && tx != ValueType::TYPE_UNKNOWN) {
        report(node->token, "Type mismatch in assignment operation");
    }
}

void Sema::visitPrint(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    checkExpr(node->children[0]);
}

void Sema::visitIf(const ASTNodePtr& node) {
    if (!node || node->children.size() < 2) return;

    ValueType condType = checkExpr(node->children[0]);
    if (condType != ValueType::TYPE_BOOL && condType != ValueType::TYPE_UNKNOWN)
        report(node->token, "If condition must be boolean");

    visit(node->children[1]);
    if (node->children.size() >= 3) visit(node->children[2]);
}

void Sema::visitWhile(const ASTNodePtr& node) {
    if (!node || node->children.size() < 2) return;

    ValueType condType = checkExpr(node->children[0]);
    if (condType != ValueType::TYPE_BOOL && condType != ValueType::TYPE_UNKNOWN)
        report(node->token, "While condition must be boolean");

    visit(node->children[1]);
}

void Sema::visitFor(const ASTNodePtr& node) {
    if (!node || node->children.size() < 4) return;

    visit(node->children[0]);

    auto condWrap = node->children[1];
    ValueType condType = ValueType::TYPE_UNKNOWN;
    if (condWrap && !condWrap->children.empty()) condType = checkExpr(condWrap->children[0]);
    if (condType != ValueType::TYPE_BOOL && condType != ValueType::TYPE_UNKNOWN)
        report(node->token, "For condition must be boolean");

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

// =============================
// Expression checking
// =============================
ValueType Sema::checkExpr(const ASTNodePtr& node) {
    if (!node) return ValueType::TYPE_UNKNOWN;

    switch (node->type) {
        case ASTNodeType::Expr:
        case ASTNodeType::CondExpr:
            if (!node->children.empty()) return checkExpr(node->children[0]);
            return ValueType::TYPE_UNKNOWN;

        case ASTNodeType::BoolOr:
        case ASTNodeType::BoolAnd: {
            ValueType L = checkExpr(node->children[0]);
            ValueType R = checkExpr(node->children[1]);
            if (L != ValueType::TYPE_BOOL || R != ValueType::TYPE_BOOL)
                report(node->token, "Logical operands must be bool");
            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::RelExpr: {
            ValueType L = checkExpr(node->children[0]);
            ValueType R = checkExpr(node->children[1]);
            if (L != ValueType::TYPE_UNKNOWN && R != ValueType::TYPE_UNKNOWN && L != R)
                report(node->token, "Operands of comparison must have same type");
            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::ArithExpr:
        case ASTNodeType::Term:
        case ASTNodeType::Power: {
            ValueType L = checkExpr(node->children[0]);
            if (node->children.size() == 1) return L;

            ValueType R = checkExpr(node->children[1]);

            bool Lnum = (L == ValueType::TYPE_INT || L == ValueType::TYPE_FLOAT);
            bool Rnum = (R == ValueType::TYPE_INT || R == ValueType::TYPE_FLOAT);
            if (!Lnum || !Rnum) report(node->token, "Arithmetic operands must be numeric");

            return (L == ValueType::TYPE_FLOAT || R == ValueType::TYPE_FLOAT)
                     ? ValueType::TYPE_FLOAT
                     : ValueType::TYPE_INT;
        }

        case ASTNodeType::Identifier: {
            std::string name = node->token.lexeme;
            if (!isVarDefined(name, node->token)) return ValueType::TYPE_UNKNOWN;
            return symbols[name];
        }

        case ASTNodeType::IntLiteral: return ValueType::TYPE_INT;
        case ASTNodeType::FloatLiteral: return ValueType::TYPE_FLOAT;
        case ASTNodeType::BoolLiteral: return ValueType::TYPE_BOOL;

        default:
            return ValueType::TYPE_UNKNOWN;
    }
}
