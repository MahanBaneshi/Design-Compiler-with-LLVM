// parser.cpp
#include "parser.h"

// =============================
// Constructor & helpers
// =============================

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), errors(), panicMode(false) {}

const Token& Parser::peek(int offset) const {
    std::size_t index = current + static_cast<std::size_t>(offset);
    if (index >= tokens.size()) return tokens.back();
    return tokens[index];
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

bool Parser::check(TokenType type, int offset) const {
    if (isAtEnd()) return false;
    return peek(offset).type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    const Token& tok = peek();
    if (tok.type == type) return advance();
    error(tok, message);
}

void Parser::error(const Token& token, const std::string& message) {
    errors.push_back(ParseError{ token.line, token.column, message });
    panicMode = true;
    throw ParseException("parse error");
}

void Parser::synchronize() {
    if (!panicMode) return;

    while (!isAtEnd()) {
        if (current > 0 && previous().type == TokenType::SEMICOLON) {
            panicMode = false;
            return;
        }

        switch (peek().type) {
            case TokenType::KW_VAR:
            case TokenType::KW_ARRAY:
            case TokenType::KW_IF:
            case TokenType::KW_WHILE:
            case TokenType::KW_FOR:
            case TokenType::KW_FOREACH:
            case TokenType::KW_PRINT:
            case TokenType::KW_ADD:
            case TokenType::KW_SUB:
            case TokenType::KW_MUL:
            case TokenType::KW_DIV:
            case TokenType::KW_MOD:
            case TokenType::KW_INC:
            case TokenType::KW_DEC:
            case TokenType::KW_PLE:
            case TokenType::KW_MIE:
            case TokenType::KW_AND:
            case TokenType::KW_OR:
            case TokenType::RBRACE:
                panicMode = false;
                return;
            default:
                break;
        }

        advance();
    }

    panicMode = false;
}

ASTNodePtr Parser::makeLeaf(ASTNodeType type, const Token& tok) {
    return std::make_shared<ASTNode>(type, tok);
}

ASTNodePtr Parser::makeNode(ASTNodeType type, const Token& tok,
                            std::initializer_list<ASTNodePtr> children) {
    auto node = std::make_shared<ASTNode>(type, tok);
    node->children.insert(node->children.end(), children.begin(), children.end());
    return node;
}

// =============================
// Entry point
// =============================

ASTNodePtr Parser::parseProgram() {
    auto startTok = peek();
    auto node = std::make_shared<ASTNode>(ASTNodeType::Program, startTok);

    if (!isAtEnd()) {
        try {
            node->children.push_back(parseDeclOrStmtList());
        } catch (const ParseException&) {
            synchronize();
        }
    }

    if (peek().type != TokenType::END_OF_FILE) {
        try { error(peek(), "Expected end of file"); }
        catch (const ParseException&) {}
    }

    return node;
}

ASTNodePtr Parser::parseProgramInternal() {
    return parseProgram();
}

// =============================
// Program / DeclOrStmtList
// =============================

ASTNodePtr Parser::parseDeclOrStmtList() {
    auto startTok = peek();
    auto listNode = std::make_shared<ASTNode>(ASTNodeType::StmtList, startTok);

    while (!isAtEnd() && !check(TokenType::RBRACE)) {
        try {
            listNode->children.push_back(parseDeclOrStmt());
        } catch (const ParseException&) {
            synchronize();
        }
    }

    return listNode;
}

ASTNodePtr Parser::parseDeclOrStmt() {
    if (check(TokenType::KW_VAR)) return parseVarDecl();
    if (check(TokenType::KW_ARRAY)) return parseArrayDecl();
    return parseStmt();
}

static bool isAssignOpKw(TokenType t) {
    switch (t) {
        case TokenType::KW_ADD:
        case TokenType::KW_SUB:
        case TokenType::KW_MUL:
        case TokenType::KW_DIV:
        case TokenType::KW_MOD:
        case TokenType::KW_INC:
        case TokenType::KW_DEC:
        case TokenType::KW_PLE:
        case TokenType::KW_MIE:
        case TokenType::KW_AND:
        case TokenType::KW_OR:
            return true;
        default:
            return false;
    }
}

ASTNodePtr Parser::parseStmt() {
    if (isAssignOpKw(peek().type)) {
        return parseAssignOpStmt();
    }
    if (check(TokenType::KW_PRINT)) return parsePrintStmt();
    if (check(TokenType::KW_IF)) return parseIfStmt();
    if (check(TokenType::KW_WHILE)) return parseWhileStmt();
    if (check(TokenType::KW_FOR)) return parseForStmt();
    if (check(TokenType::KW_FOREACH)) return parseForeachStmt();
    if (check(TokenType::LBRACE)) return parseBlock();

    error(peek(), "Unexpected token in statement");
}

// =============================
// VarDecl / ArrayDecl
// =============================

ASTNodePtr Parser::parseVarDecl() {
    Token startTok = expect(TokenType::KW_VAR, "Expected 'var'");
    auto node = std::make_shared<ASTNode>(ASTNodeType::VarDecl, startTok);
    node->children.push_back(parseVarDeclList());
    expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

ASTNodePtr Parser::parseVarDeclList() {
    auto startTok = peek();
    auto listNode = std::make_shared<ASTNode>(ASTNodeType::VarDeclItem, startTok);

    listNode->children.push_back(parseVarDeclItem());

    while (match(TokenType::COMMA)) {
        listNode->children.push_back(parseVarDeclItem());
    }

    return listNode;
}

ASTNodePtr Parser::parseVarDeclItem() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier in variable declaration");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

    auto typeNode = parseType();

    auto varItemNode = std::make_shared<ASTNode>(ASTNodeType::VarDeclItem, idTok);
    varItemNode->children.push_back(idNode);
    varItemNode->children.push_back(typeNode);

    if (match(TokenType::ASSIGN)) {
        auto exprNode = parseExpr();
        varItemNode->children.push_back(exprNode);
    }

    return varItemNode;
}

ASTNodePtr Parser::parseType() {
    if (match(TokenType::KW_INT) || match(TokenType::KW_FLOAT) || match(TokenType::KW_BOOL)) {
        return makeLeaf(ASTNodeType::Identifier, previous());
    }
    error(peek(), "Expected type (int, float, bool)");
}

ASTNodePtr Parser::parseArrayDecl() {
    Token startTok = expect(TokenType::KW_ARRAY, "Expected 'array'");
    Token idTok = expect(TokenType::IDENTIFIER, "Expected array name");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

    expect(TokenType::ASSIGN, "Expected '=' after array name");
    auto litNode = parseArrayLiteral();
    expect(TokenType::SEMICOLON, "Expected ';' after array declaration");

    auto node = std::make_shared<ASTNode>(ASTNodeType::ArrayDecl, startTok);
    node->children.push_back(idNode);
    node->children.push_back(litNode);
    return node;
}

ASTNodePtr Parser::parseArrayLiteral() {
    Token startTok = expect(TokenType::LBRACKET, "Expected '[' for array literal");
    auto node = std::make_shared<ASTNode>(ASTNodeType::ArrayLiteral, startTok);
    if (!check(TokenType::RBRACKET)) node->children.push_back(parseExprList());
    expect(TokenType::RBRACKET, "Expected ']' at end of array literal");
    return node;
}

ASTNodePtr Parser::parseExprList() {
    auto startTok = peek();
    auto listNode = std::make_shared<ASTNode>(ASTNodeType::Expr, startTok);
    listNode->children.push_back(parseExpr());
    while (match(TokenType::COMMA)) {
        listNode->children.push_back(parseExpr());
    }
    return listNode;
}

// =============================
// Assignment-style statements (ADD/SUB/...)
// =============================

static bool isThreeIdOp(TokenType t) {
    return t == TokenType::KW_ADD || t == TokenType::KW_SUB || t == TokenType::KW_MUL ||
           t == TokenType::KW_DIV || t == TokenType::KW_MOD || t == TokenType::KW_AND ||
           t == TokenType::KW_OR;
}
static bool isTwoIdOp(TokenType t) {
    return t == TokenType::KW_PLE || t == TokenType::KW_MIE;
}
static bool isOneIdOp(TokenType t) {
    return t == TokenType::KW_INC || t == TokenType::KW_DEC;
}

static const char* assignOpExpectedMsg(TokenType t) {
    if (isThreeIdOp(t)) return "Expected: OP dst src1 src2";
    if (isTwoIdOp(t))   return "Expected: OP dst src";
    if (isOneIdOp(t))   return "Expected: OP dst";
    return "Expected assignment operation";
}

// dst: IDENTIFIER or IDENTIFIER '[' Expr ']'
ASTNodePtr Parser::parseAssignOpDst() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected destination identifier");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

    if (match(TokenType::LBRACKET)) {
        auto idx = parseExpr();
        expect(TokenType::RBRACKET, "Expected ']' after destination index");

        auto node = std::make_shared<ASTNode>(ASTNodeType::Factor, idTok);
        node->children.push_back(idNode);
        node->children.push_back(idx);
        return node;
    }

    return idNode;
}

// src: Factor (identifier / literal / arr[idx] / builtin / unary +/- / (expr))
ASTNodePtr Parser::parseAssignOpSrc() {
    return parseFactor();
}

ASTNodePtr Parser::parseAssignOpStmt() {
    Token opTok = peek();
    if (!isAssignOpKw(opTok.type)) {
        error(peek(), "Expected assignment operation keyword");
    }
    advance(); // consume op keyword

    auto node = std::make_shared<ASTNode>(ASTNodeType::AssignOpStmt, opTok);

    node->children.push_back(parseAssignOpDst());

    if (isOneIdOp(opTok.type)) {
        expect(TokenType::SEMICOLON, "Expected ';' after operation");
        return node;
    }

    node->children.push_back(parseAssignOpSrc());

    if (isTwoIdOp(opTok.type)) {
        expect(TokenType::SEMICOLON, "Expected ';' after operation");
        return node;
    }

    node->children.push_back(parseAssignOpSrc());

    expect(TokenType::SEMICOLON, "Expected ';' after operation");
    return node;
}

// =============================
// Print
// =============================

ASTNodePtr Parser::parsePrintStmt() {
    Token kw = expect(TokenType::KW_PRINT, "Expected 'print'");
    expect(TokenType::LPAREN, "Expected '(' after 'print'");
    auto exprNode = parseExpr();
    expect(TokenType::RPAREN, "Expected ')' after print argument");
    expect(TokenType::SEMICOLON, "Expected ';' after print statement");

    auto node = std::make_shared<ASTNode>(ASTNodeType::PrintStmt, kw);
    node->children.push_back(exprNode);
    return node;
}

// =============================
// Block / StmtList
// =============================

ASTNodePtr Parser::parseBlock() {
    Token lbrace = expect(TokenType::LBRACE, "Expected '{'");
    auto node = std::make_shared<ASTNode>(ASTNodeType::Block, lbrace);
    node->children.push_back(parseBlockBody());
    expect(TokenType::RBRACE, "Expected '}' at end of block");
    return node;
}

ASTNodePtr Parser::parseBlockBody() {
    auto node = std::make_shared<ASTNode>(ASTNodeType::StmtList, peek());
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        try {
            node->children.push_back(parseStmt());
        } catch (const ParseException&) {
            synchronize();
        }
    }
    return node;
}

ASTNodePtr Parser::parseStmtList() {
    auto node = std::make_shared<ASTNode>(ASTNodeType::StmtList, peek());
    while (!isAtEnd() && !check(TokenType::RBRACE)) {
        try {
            node->children.push_back(parseStmt());
        } catch (const ParseException&) {
            synchronize();
        }
    }
    return node;
}

// =============================
// If / While / For / Foreach
// =============================

ASTNodePtr Parser::parseIfStmt() {
    Token ifTok = expect(TokenType::KW_IF, "Expected 'if'");
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parseBoolExpr();
    expect(TokenType::RPAREN, "Expected ')' after if condition");
    auto thenBlock = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::IfStmt, ifTok);
    node->children.push_back(cond);
    node->children.push_back(thenBlock);

    if (check(TokenType::KW_ELSE)) node->children.push_back(parseIfElsePart());
    return node;
}

ASTNodePtr Parser::parseIfElsePart() {
    Token elseTok = expect(TokenType::KW_ELSE, "Expected 'else'");
    if (check(TokenType::KW_IF)) {
        auto elseIfNode = parseIfStmt();
        auto node = std::make_shared<ASTNode>(ASTNodeType::IfStmt, elseTok);
        node->children.push_back(elseIfNode);
        return node;
    } else {
        auto block = parseBlock();
        auto node = std::make_shared<ASTNode>(ASTNodeType::IfStmt, elseTok);
        node->children.push_back(block);
        return node;
    }
}

ASTNodePtr Parser::parseWhileStmt() {
    Token wTok = expect(TokenType::KW_WHILE, "Expected 'while'");
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto cond = parseBoolExpr();
    expect(TokenType::RPAREN, "Expected ')' after while condition");
    auto body = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::WhileStmt, wTok);
    node->children.push_back(cond);
    node->children.push_back(body);
    return node;
}

ASTNodePtr Parser::parseForStmt() {
    Token fTok = expect(TokenType::KW_FOR, "Expected 'for'");
    expect(TokenType::LPAREN, "Expected '(' after 'for'");
    auto init = parseForInit();
    expect(TokenType::SEMICOLON, "Expected ';' after for init");
    auto cond = parseForCond();
    expect(TokenType::SEMICOLON, "Expected ';' after for condition");
    auto update = parseForUpdate();
    expect(TokenType::RPAREN, "Expected ')' after for header");
    auto body = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForStmt, fTok);
    node->children.push_back(init);
    node->children.push_back(cond);
    node->children.push_back(update);
    node->children.push_back(body);
    return node;
}

ASTNodePtr Parser::parseForeachStmt() {
    Token fTok = expect(TokenType::KW_FOREACH, "Expected 'foreach'");
    expect(TokenType::LPAREN, "Expected '(' after 'foreach'");
    Token varTok = expect(TokenType::IDENTIFIER, "Expected identifier in foreach");
    auto varNode = makeLeaf(ASTNodeType::Identifier, varTok);
    expect(TokenType::KW_IN, "Expected 'in' in foreach");
    Token arrTok = expect(TokenType::IDENTIFIER, "Expected collection identifier in foreach");
    auto arrNode = makeLeaf(ASTNodeType::Identifier, arrTok);
    expect(TokenType::RPAREN, "Expected ')' after foreach header");
    auto body = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForeachStmt, fTok);
    node->children.push_back(varNode);
    node->children.push_back(arrNode);
    node->children.push_back(body);
    return node;
}

// For parts
ASTNodePtr Parser::parseForInit() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier in for init");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);
    Token assignTok = expect(TokenType::ASSIGN, "Expected '=' in for init");
    auto expr = parseArithExpr();

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForInit, assignTok);
    node->children.push_back(idNode);
    node->children.push_back(expr);
    return node;
}

ASTNodePtr Parser::parseForCond() {
    auto expr = parseBoolExpr();
    auto node = std::make_shared<ASTNode>(ASTNodeType::ForCond, expr->token);
    node->children.push_back(expr);
    return node;
}

static bool isUpdateKw(TokenType t) {
    return isAssignOpKw(t);
}

ASTNodePtr Parser::parseForUpdate() {
    if (!isUpdateKw(peek().type)) {
        error(peek(), "Expected for update (INC/DEC/ADD/SUB/MUL/DIV/MOD/PLE/MIE/AND/OR)");
    }

    Token opTok = peek();
    advance(); // consume op

    auto opNode = std::make_shared<ASTNode>(ASTNodeType::AssignOpStmt, opTok);

    opNode->children.push_back(parseAssignOpDst());

    if (isOneIdOp(opTok.type)) {
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
        node->children.push_back(opNode);
        return node;
    }

    opNode->children.push_back(parseAssignOpSrc());

    if (isTwoIdOp(opTok.type)) {
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
        node->children.push_back(opNode);
        return node;
    }

    opNode->children.push_back(parseAssignOpSrc());

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
    node->children.push_back(opNode);
    return node;
}

// =============================
// Expressions (precedence)
// =============================

ASTNodePtr Parser::parseExpr() {
    auto be = parseBoolExpr();
    auto node = std::make_shared<ASTNode>(ASTNodeType::Expr, be->token);
    node->children.push_back(be);
    return node;
}

ASTNodePtr Parser::parseBoolExpr() {
    auto node = parseBoolOr();
    auto wrap = std::make_shared<ASTNode>(ASTNodeType::BoolExpr, node->token);
    wrap->children.push_back(node);
    return wrap;
}

ASTNodePtr Parser::parseBoolOr() {
    auto left = parseBoolAnd();
    while (match(TokenType::LOGIC_OR)) {
        Token opTok = previous();
        auto right = parseBoolAnd();
        auto node = std::make_shared<ASTNode>(ASTNodeType::BoolOr, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseBoolAnd() {
    auto left = parseRelOrArith();
    while (match(TokenType::LOGIC_AND)) {
        Token opTok = previous();
        auto right = parseRelOrArith();
        auto node = std::make_shared<ASTNode>(ASTNodeType::BoolAnd, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

// RelOrArith: ArithExpr (RelOp ArithExpr)?
ASTNodePtr Parser::parseRelOrArith() {
    auto left = parseArithExpr();
    if (check(TokenType::LT) || check(TokenType::GT) ||
        check(TokenType::LTE) || check(TokenType::GTE) ||
        check(TokenType::EQ) || check(TokenType::NEQ)) {
        Token opTok = advance();
        auto right = parseArithExpr();
        auto node = std::make_shared<ASTNode>(ASTNodeType::RelExpr, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        return node;
    }
    return left;
}

ASTNodePtr Parser::parseArithExpr() {
    auto left = parseTerm();
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        Token opTok = previous();
        auto right = parseTerm();
        auto node = std::make_shared<ASTNode>(ASTNodeType::ArithExpr, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parseTerm() {
    auto left = parsePower();
    while (match(TokenType::STAR) || match(TokenType::SLASH) || match(TokenType::PERCENT)) {
        Token opTok = previous();
        auto right = parsePower();
        auto node = std::make_shared<ASTNode>(ASTNodeType::Term, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

ASTNodePtr Parser::parsePower() {
    auto base = parseFactor();
    if (match(TokenType::CARET)) {
        Token opTok = previous();
        auto exponent = parsePower();
        auto node = std::make_shared<ASTNode>(ASTNodeType::Power, opTok);
        node->children.push_back(base);
        node->children.push_back(exponent);
        return node;
    }
    return base;
}

ASTNodePtr Parser::parseFactor() {
    if (match(TokenType::BOOL_LITERAL)) return makeLeaf(ASTNodeType::BoolLiteral, previous());
    if (match(TokenType::INT_LITERAL))  return makeLeaf(ASTNodeType::IntLiteral, previous());
    if (match(TokenType::FLOAT_LITERAL))return makeLeaf(ASTNodeType::FloatLiteral, previous());

    if (check(TokenType::KW_TO_INT) || check(TokenType::KW_TO_FLOAT) ||
        check(TokenType::KW_TO_BOOL) || check(TokenType::KW_ABS) ||
        check(TokenType::KW_LENGTH) || check(TokenType::KW_MAX) ||
        check(TokenType::KW_INDEX) || check(TokenType::KW_FIND)) {
        return parseBuiltinCall();
    }

    if (match(TokenType::IDENTIFIER)) {
        Token idTok = previous();
        auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

        if (match(TokenType::LBRACKET)) {
            auto indexExpr = parseExpr();
            expect(TokenType::RBRACKET, "Expected ']' after array index");
            auto node = std::make_shared<ASTNode>(ASTNodeType::Factor, idTok);
            node->children.push_back(idNode);
            node->children.push_back(indexExpr);
            return node;
        }

        return idNode;
    }

    if (match(TokenType::LPAREN)) {
        auto expr = parseExpr();
        expect(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    if (match(TokenType::PLUS)) {
        auto expr = parseFactor();
        auto node = std::make_shared<ASTNode>(ASTNodeType::Factor, previous());
        node->children.push_back(expr);
        return node;
    }

    if (match(TokenType::MINUS)) {
        auto expr = parseFactor();
        auto node = std::make_shared<ASTNode>(ASTNodeType::Factor, previous());
        node->children.push_back(expr);
        return node;
    }

    error(peek(), "Expected factor");
}

// =============================
// Builtin calls
// =============================

ASTNodePtr Parser::parseBuiltinCall() {
    auto builtinName = parseBuiltinName();
    expect(TokenType::LPAREN, "Expected '(' after builtin function");
    auto node = std::make_shared<ASTNode>(ASTNodeType::BuiltinCall, builtinName->token);
    if (!check(TokenType::RPAREN)) node->children.push_back(parseArgList());
    expect(TokenType::RPAREN, "Expected ')' after builtin call");
    return node;
}

ASTNodePtr Parser::parseBuiltinName() {
    if (match(TokenType::KW_TO_INT) || match(TokenType::KW_TO_FLOAT) ||
        match(TokenType::KW_TO_BOOL) || match(TokenType::KW_ABS) ||
        match(TokenType::KW_LENGTH) || match(TokenType::KW_MAX) ||
        match(TokenType::KW_INDEX) || match(TokenType::KW_FIND)) {
        return makeLeaf(ASTNodeType::Identifier, previous());
    }
    error(peek(), "Expected builtin function name");
}

ASTNodePtr Parser::parseArgList() {
    auto node = std::make_shared<ASTNode>(ASTNodeType::ArgList, peek());
    node->children.push_back(parseExpr());
    while (match(TokenType::COMMA)) node->children.push_back(parseExpr());
    return node;
}
