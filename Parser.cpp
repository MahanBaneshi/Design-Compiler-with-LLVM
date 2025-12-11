#include "parser.h"

// =============================
// Constructor & helpers
// =============================

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0), errors(), panicMode(false) {}

const Token& Parser::peek(int offset) const {
    std::size_t index = current + static_cast<std::size_t>(offset);
    if (index >= tokens.size()) {
        return tokens.back(); // باید END_OF_FILE باشد
    }
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
    if (!isAtEnd()) {
        current++;
    }
    return previous();
}

const Token& Parser::previous() const {
    return tokens[current - 1];
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    const Token& tok = peek();
    if (tok.type == type) {
        return advance();
    }
    error(tok, message);
    // error همیشه استثناء می‌اندازد، پس اینجا برنمی‌گردیم
}

void Parser::error(const Token& token, const std::string& message) {
    // خطا را ذخیره کن
    errors.push_back(ParseError{
        token.line,
        token.column,
        message
    });

    // وارد حالت panic می‌شویم؛ بعداً synchronize کمک می‌کند
    panicMode = true;

    // استثناء سبک برای بیرون پریدن از تابع فعلی
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

    // اگر هنوز روی EOF نیستیم، خودش یک خطاست
    if (peek().type != TokenType::END_OF_FILE) {
        try {
            error(peek(), "Expected end of file");
        } catch (const ParseException&) {
            // خطا ثبت شد، نیازی به sync دوباره نیست
        }
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
    if (check(TokenType::KW_VAR)) {
        return parseVarDecl();
    } else if (check(TokenType::KW_ARRAY)) {
        return parseArrayDecl();
    } else {
        return parseStmt();
    }
}

ASTNodePtr Parser::parseStmt() {
    if (check(TokenType::IDENTIFIER)) {
        // تشخیص: اگر بعد از IDENTIFIER، ++ یا -- بیاید => UnaryStmt
        if (check(TokenType::PLUS_PLUS, 1) || check(TokenType::MINUS_MINUS, 1)) {
            return parseUnaryStmt();
        }
        return parseAssignStmt();
    }
    if (check(TokenType::KW_PRINT)) {
        return parsePrintStmt();
    }
    if (check(TokenType::KW_IF)) {
        return parseIfStmt();
    }
    if (check(TokenType::KW_WHILE)) {
        return parseWhileStmt();
    }
    if (check(TokenType::KW_FOR)) {
        return parseForStmt();
    }
    if (check(TokenType::KW_FOREACH)) {
        return parseForeachStmt();
    }
    if (check(TokenType::LBRACE)) {
        return parseBlock();
    }

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
    return varItemNode;
}

ASTNodePtr Parser::parseType() {
    if (match(TokenType::KW_INT) || match(TokenType::KW_FLOAT) || match(TokenType::KW_BOOL)) {
        // نوع را به صورت یک leaf ساده نگه می‌داریم
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
    if (!check(TokenType::RBRACKET)) {
        node->children.push_back(parseExprList());
    }
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
// Statements
// =============================

ASTNodePtr Parser::parseAssignStmt() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");

    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

    ASTNodePtr indexNode = nullptr;
    if (match(TokenType::LBRACKET)) {
        auto idxExpr = parseExpr();
        expect(TokenType::RBRACKET, "Expected ']' in array indexing");
        indexNode = idxExpr;
    }

    Token opTok = peek();
    if (!match(TokenType::ASSIGN) &&
        !match(TokenType::PLUS_EQ) &&
        !match(TokenType::MINUS_EQ) &&
        !match(TokenType::STAR_EQ) &&
        !match(TokenType::SLASH_EQ) &&
        !match(TokenType::PERCENT_EQ)) {
        error(peek(), "Expected assignment operator (=, +=, -=, *=, /=, %=)");
    }

    auto exprNode = parseExpr();
    expect(TokenType::SEMICOLON, "Expected ';' after assignment");

    auto node = std::make_shared<ASTNode>(ASTNodeType::AssignStmt, opTok);
    node->children.push_back(idNode);
    if (indexNode) node->children.push_back(indexNode);
    node->children.push_back(exprNode);
    return node;
}

ASTNodePtr Parser::parseUnaryStmt() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

    Token opTok = peek();
    if (!match(TokenType::PLUS_PLUS) && !match(TokenType::MINUS_MINUS)) {
        error(peek(), "Expected '++' or '--'");
    }
    expect(TokenType::SEMICOLON, "Expected ';' after unary statement");

    auto node = std::make_shared<ASTNode>(ASTNodeType::UnaryStmt, opTok);
    node->children.push_back(idNode);
    return node;
}

ASTNodePtr Parser::parsePrintStmt() {
    Token kw = expect(TokenType::KW_PRINT, "Expected 'print'");
    expect(TokenType::LPAREN, "Expected '(' after 'print'");

    // آرگومان print یک Expr کامل است
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

    if (check(TokenType::KW_ELSE)) {
        node->children.push_back(parseIfElsePart());
    }
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

ASTNodePtr Parser::parseForUpdate() {
    if (check(TokenType::IDENTIFIER) &&
        (check(TokenType::PLUS_PLUS, 1) || check(TokenType::MINUS_MINUS, 1))) {
        auto u = parseUnaryExpr();
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, u->token);
        node->children.push_back(u);
        return node;
    } else {
        Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier in for update");
        auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);
        Token opTok = peek();
        if (!match(TokenType::ASSIGN) &&
            !match(TokenType::PLUS_EQ) &&
            !match(TokenType::MINUS_EQ) &&
            !match(TokenType::STAR_EQ) &&
            !match(TokenType::SLASH_EQ) &&
            !match(TokenType::PERCENT_EQ)) {
            error(peek(), "Expected assignment operator in for update");
        }
        auto expr = parseExpr();
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
        node->children.push_back(idNode);
        node->children.push_back(expr);
        return node;
    }
}

ASTNodePtr Parser::parseUnaryExpr() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);
    Token opTok = peek();
    if (!match(TokenType::PLUS_PLUS) && !match(TokenType::MINUS_MINUS)) {
        error(peek(), "Expected ++ or --");
    }
    auto node = std::make_shared<ASTNode>(ASTNodeType::UnaryOp, opTok);
    node->children.push_back(idNode);
    return node;
}

// =============================
// Expressions (precedence)
// =============================

// Expr -> BoolExpr
ASTNodePtr Parser::parseExpr() {
    auto be = parseBoolExpr();
    auto node = std::make_shared<ASTNode>(ASTNodeType::Expr, be->token);
    node->children.push_back(be);
    return node;
}

// BoolExpr -> BoolOr
ASTNodePtr Parser::parseBoolExpr() {
    auto node = parseBoolOr();
    auto wrap = std::make_shared<ASTNode>(ASTNodeType::BoolExpr, node->token);
    wrap->children.push_back(node);
    return wrap;
}

// BoolOr -> BoolAnd (LOGIC_OR BoolAnd)*
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

// BoolAnd -> RelOrArith (LOGIC_AND RelOrArith)*
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

// RelOrArith:
//   BitOr ( (LT|GT|LTE|GTE|EQ|NEQ) BitOr )?
ASTNodePtr Parser::parseRelOrArith() {
    auto left = parseBitOr();
    if (check(TokenType::LT) || check(TokenType::GT) ||
        check(TokenType::LTE) || check(TokenType::GTE) ||
        check(TokenType::EQ) || check(TokenType::NEQ)) {
        Token opTok = advance();
        auto right = parseBitOr();
        auto node = std::make_shared<ASTNode>(ASTNodeType::RelExpr, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        return node;
    }
    return left;
}

// BitOr -> BitAnd (BIT_OR BitAnd)*
ASTNodePtr Parser::parseBitOr() {
    auto left = parseBitAnd();
    while (match(TokenType::BIT_OR)) {
        Token opTok = previous();
        auto right = parseBitAnd();
        auto node = std::make_shared<ASTNode>(ASTNodeType::BitOr, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

// BitAnd -> ArithExpr (BIT_AND ArithExpr)*
ASTNodePtr Parser::parseBitAnd() {
    auto left = parseArithExpr();
    while (match(TokenType::BIT_AND)) {
        Token opTok = previous();
        auto right = parseArithExpr();
        auto node = std::make_shared<ASTNode>(ASTNodeType::BitAnd, opTok);
        node->children.push_back(left);
        node->children.push_back(right);
        left = node;
    }
    return left;
}

// ArithExpr -> Term (('+'|'-') Term)*
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

// Term -> Power (('*'|'/'|'%') Power)*
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

// Power -> Factor ('^' Power)?
ASTNodePtr Parser::parsePower() {
    auto base = parseFactor();
    if (match(TokenType::CARET)) {
        Token opTok = previous();
        auto exponent = parsePower(); // راست‌وابسته
        auto node = std::make_shared<ASTNode>(ASTNodeType::Power, opTok);
        node->children.push_back(base);
        node->children.push_back(exponent);
        return node;
    }
    return base;
}

// Factor
ASTNodePtr Parser::parseFactor() {
    if (match(TokenType::BOOL_LITERAL)) {
        return makeLeaf(ASTNodeType::BoolLiteral, previous());
    }
    if (match(TokenType::INT_LITERAL)) {
        return makeLeaf(ASTNodeType::IntLiteral, previous());
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        return makeLeaf(ASTNodeType::FloatLiteral, previous());
    }

    // Builtin functions
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

    // پرانتز: یک Expr کامل را می‌خوانیم (نه فقط ArithExpr)
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpr();
        expect(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }

    // unary +
    if (match(TokenType::PLUS)) {
        auto expr = parseFactor();
        auto node = std::make_shared<ASTNode>(ASTNodeType::UnaryOp, previous());
        node->children.push_back(expr);
        return node;
    }

    // unary -
    if (match(TokenType::MINUS)) {
        auto expr = parseFactor();
        auto node = std::make_shared<ASTNode>(ASTNodeType::UnaryOp, previous());
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
    if (!check(TokenType::RPAREN)) {
        node->children.push_back(parseArgList());
    }
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
    while (match(TokenType::COMMA)) {
        node->children.push_back(parseExpr());
    }
    return node;
}
