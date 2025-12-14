// Sema.cpp:
#include "Sema.h"

Sema::Sema(const ASTNodePtr& root)
    : root(root) {}

bool Sema::analyze() {
    visit(root);
    return !hasError;
}

void Sema::report(const Token& tok, const std::string& msg) {
    std::cerr << "Semantic error at line " << tok.line
              << ", col " << tok.column << ": " << msg << "\n";
    hasError = true;
}

// =============================
// Symbol Table Helpers
// =============================

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
            // ForInit/ForUpdate nodes wrap other nodes; just visit children
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
    auto list = node->children[0];

    for (auto& item : list->children) {
        auto idNode = item->children[0];
        auto typeNode = item->children[1];

        std::string name = idNode->token.lexeme;
        std::string typeStr = typeNode->token.lexeme;

        if (symbols.count(name)) {
            report(idNode->token, "Redefinition of variable '" + name + "'");
            continue;
        }

        symbols[name] = stringToType(typeStr);

        // optional initializer: '=' Expr
        if (item->children.size() >= 3) {
            ValueType rhsType = checkExpr(item->children[2]);
            if (rhsType != ValueType::TYPE_UNKNOWN && symbols[name] != rhsType) {
                report(idNode->token, "Type mismatch in initialization of '" + name + "'");
            }
        }
    }
}

void Sema::visitArrayDecl(const ASTNodePtr& node) {
    auto idNode = node->children[0];
    std::string name = idNode->token.lexeme;

    if (symbols.count(name))
        report(idNode->token, "Redefinition of array '" + name + "'");

    symbols[name] = ValueType::TYPE_ARRAY;
}

// AssignOpStmt node->token is KW_ADD/KW_SUB/... and children are identifiers:
//  INC/DEC: [x]
//  PLE/MIE: [x, y]
//  ADD/SUB/MUL/DIV/MOD/AND/OR: [x, y, z]
void Sema::visitAssignOpStmt(const ASTNodePtr& node) {
    auto opType = node->token.type;

    auto needIdent = [&](int i, const char* what) -> std::string {
        if (i >= (int)node->children.size() || node->children[i]->type != ASTNodeType::Identifier) {
            report(node->token, std::string("Malformed ") + what);
            return "";
        }
        return node->children[i]->token.lexeme;
    };

    std::string x = needIdent(0, "assignment op");
    if (x.empty()) return;

    if (!isVarDefined(x, node->children[0]->token)) return;
    ValueType tx = symbols[x];

    if (opType == TokenType::KW_INC || opType == TokenType::KW_DEC) {
        if (tx != ValueType::TYPE_INT) {
            report(node->children[0]->token, "INC/DEC only allowed on int variables");
        }
        return;
    }

    std::string y = needIdent(1, "assignment op");
    if (y.empty()) return;
    if (!isVarDefined(y, node->children[1]->token)) return;
    ValueType ty = symbols[y];

    if (opType == TokenType::KW_PLE || opType == TokenType::KW_MIE) {
        // x = x +/- y : numeric only, and x must be numeric
        bool xNum = (tx == ValueType::TYPE_INT || tx == ValueType::TYPE_FLOAT);
        bool yNum = (ty == ValueType::TYPE_INT || ty == ValueType::TYPE_FLOAT);
        if (!xNum || !yNum) report(node->token, "PLE/MIE operands must be numeric");
        // keep type of x; but forbid mixing bool/array etc already handled above
        return;
    }

    std::string z = needIdent(2, "assignment op");
    if (z.empty()) return;
    if (!isVarDefined(z, node->children[2]->token)) return;
    ValueType tz = symbols[z];

    if (opType == TokenType::KW_AND || opType == TokenType::KW_OR) {
        // spec: AND/OR only for bool variables
        if (tx != ValueType::TYPE_BOOL || ty != ValueType::TYPE_BOOL || tz != ValueType::TYPE_BOOL) {
            report(node->token, "AND/OR only allowed on bool variables");
        }
        return;
    }

    // ADD/SUB/MUL/DIV/MOD: numeric only
    bool yNum = (ty == ValueType::TYPE_INT || ty == ValueType::TYPE_FLOAT);
    bool zNum = (tz == ValueType::TYPE_INT || tz == ValueType::TYPE_FLOAT);
    if (!yNum || !zNum) {
        report(node->token, "Arithmetic operands must be numeric");
        return;
    }

    // x must be numeric too
    bool xNum = (tx == ValueType::TYPE_INT || tx == ValueType::TYPE_FLOAT);
    if (!xNum) {
        report(node->token, "Assignment target must be numeric for arithmetic operation");
        return;
    }

    // result type: float if any operand is float, else int
    ValueType result = (ty == ValueType::TYPE_FLOAT || tz == ValueType::TYPE_FLOAT)
                         ? ValueType::TYPE_FLOAT
                         : ValueType::TYPE_INT;

    if (tx != result) {
        report(node->token, "Type mismatch in assignment operation");
    }

    // division by zero (only if z is literal 0 and op is DIV)
    if (opType == TokenType::KW_DIV) {
        // cannot detect unless you track constants; keep only literal check for now
    }
}

void Sema::visitPrint(const ASTNodePtr& node) {
    checkExpr(node->children[0]);
}

void Sema::visitIf(const ASTNodePtr& node) {
    ValueType condType = checkExpr(node->children[0]);
    if (condType != ValueType::TYPE_BOOL)
        report(node->token, "If condition must be boolean");

    visit(node->children[1]);
    if (node->children.size() == 3)
        visit(node->children[2]);
}

void Sema::visitWhile(const ASTNodePtr& node) {
    ValueType condType = checkExpr(node->children[0]);
    if (condType != ValueType::TYPE_BOOL)
        report(node->token, "While condition must be boolean");

    visit(node->children[1]);
}

void Sema::visitFor(const ASTNodePtr& node) {
    // init: ForInit('='), children: [id, arithExpr]
    auto init = node->children[0];
    std::string name = init->children[0]->token.lexeme;

    if (!isVarDefined(name, init->children[0]->token)) return;

    // type-check init assignment: id type vs arith expr type
    ValueType lhsType = symbols[name];
    ValueType rhsType = checkExpr(init->children[1]);
    if (lhsType != rhsType)
        report(init->token, "Type mismatch in for init");

    ValueType cond = checkExpr(node->children[1]->children[0]);
    if (cond != ValueType::TYPE_BOOL)
        report(node->token, "For condition must be boolean");

    // update wraps AssignOpStmt
    visit(node->children[2]);
    visit(node->children[3]);
}

void Sema::visitForeach(const ASTNodePtr& node) {
    std::string varName = node->children[0]->token.lexeme;
    std::string arrName = node->children[1]->token.lexeme;

    // keep your previous assumption: foreach var is int
    symbols[varName] = ValueType::TYPE_INT;

    if (!isVarDefined(arrName, node->children[1]->token)) return;
    if (symbols[arrName] != ValueType::TYPE_ARRAY)
        report(node->children[1]->token, "Foreach target must be an array");

    visit(node->children[2]);
}

void Sema::visitBlock(const ASTNodePtr& node) {
    visit(node->children[0]);
}

// =============================
// Expression checking
// =============================

ValueType Sema::checkExpr(const ASTNodePtr& node) {
    if (!node) return ValueType::TYPE_UNKNOWN;

    switch (node->type) {
        case ASTNodeType::Expr:
        case ASTNodeType::BoolExpr:
            return checkExpr(node->children[0]);

        case ASTNodeType::BoolOr: {
            auto L = checkExpr(node->children[0]);
            auto R = checkExpr(node->children[1]);
            if (L != ValueType::TYPE_BOOL || R != ValueType::TYPE_BOOL)
                report(node->token, "Operands of 'or' must be bool");
            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::BoolAnd: {
            auto L = checkExpr(node->children[0]);
            auto R = checkExpr(node->children[1]);
            if (L != ValueType::TYPE_BOOL || R != ValueType::TYPE_BOOL)
                report(node->token, "Operands of 'and' must be bool");
            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::RelExpr: {
            auto L = checkExpr(node->children[0]);
            auto R = checkExpr(node->children[1]);
            if (L != R)
                report(node->token, "Operands of comparison must have same type");
            return ValueType::TYPE_BOOL;
        }

        case ASTNodeType::ArithExpr:
        case ASTNodeType::Term:
        case ASTNodeType::Power: {
            auto L = checkExpr(node->children[0]);
            ValueType result = L;

            if (node->children.size() == 2) {
                auto R = checkExpr(node->children[1]);

                // division by zero for expression-level '/' only (kept)
                if (node->type == ASTNodeType::Term &&
                    node->token.type == TokenType::SLASH) {

                    auto rhs = node->children[1];
                    if (rhs->type == ASTNodeType::IntLiteral &&
                        rhs->token.lexeme == "0") {
                        report(rhs->token, "Division by zero");
                    }
                }

                bool Lnum = (L == ValueType::TYPE_INT || L == ValueType::TYPE_FLOAT);
                bool Rnum = (R == ValueType::TYPE_INT || R == ValueType::TYPE_FLOAT);
                if (!Lnum || !Rnum) report(node->token, "Arithmetic operands must be numeric");

                result = (L == ValueType::TYPE_FLOAT || R == ValueType::TYPE_FLOAT)
                           ? ValueType::TYPE_FLOAT
                           : ValueType::TYPE_INT;
            }

            return result;
        }

        case ASTNodeType::Identifier: {
            std::string name = node->token.lexeme;
            if (!isVarDefined(name, node->token)) return ValueType::TYPE_UNKNOWN;
            return symbols[name];
        }

        case ASTNodeType::IntLiteral:   return ValueType::TYPE_INT;
        case ASTNodeType::FloatLiteral: return ValueType::TYPE_FLOAT;
        case ASTNodeType::BoolLiteral:  return ValueType::TYPE_BOOL;

        default:
            return ValueType::TYPE_UNKNOWN;
    }
}
