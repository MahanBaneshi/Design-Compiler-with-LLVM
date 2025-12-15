#include "parser.h"

Parser::Parser(const std::vector<Token>& tokens) : tokens(tokens), current(0) {}

const Token& Parser::peek(int offset) const {
    std::size_t index = current + static_cast<std::size_t>(offset);
    if (index >= tokens.size()) return tokens.back();
    return tokens[index];
}

bool Parser::isAtEnd() const { return peek().type == TokenType::END_OF_FILE; }

bool Parser::check(TokenType type, int offset) const {
    if (isAtEnd()) return false;
    return peek(offset).type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto t : types) if (check(t)) { advance(); return true; }
    return false;
}

const Token& Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

const Token& Parser::previous() const { return tokens[current - 1]; }

const Token& Parser::expect(TokenType type, const std::string& message) {
    const Token& tok = peek();
    if (tok.type == type) return advance();
    error(tok, message);
    return tok;
}

void Parser::error(const Token& token, const std::string& message) {
    errors.push_back(ParseError{ token.line, token.column, message });
    panicMode = true;
    throw ParseException(message);
}

static bool isDeclStart(TokenType t) {
    return t == TokenType::KW_VAR || t == TokenType::KW_ARRAY ||
           t == TokenType::KW_INT || t == TokenType::KW_FLOAT || t == TokenType::KW_BOOL;
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

static bool isThreeSrcOp(TokenType t) {
    return t == TokenType::KW_ADD || t == TokenType::KW_SUB || t == TokenType::KW_MUL ||
           t == TokenType::KW_DIV || t == TokenType::KW_MOD || t == TokenType::KW_AND ||
           t == TokenType::KW_OR;
}
static bool isTwoSrcOp(TokenType t) { return t == TokenType::KW_PLE || t == TokenType::KW_MIE; }
static bool isOneSrcOp(TokenType t) { return t == TokenType::KW_INC || t == TokenType::KW_DEC; }

void Parser::synchronize() {
    if (!panicMode) return;

    if (!isAtEnd()) advance(); // مهم: حتماً یک توکن جلو برو تا گیر نکنیم

    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) {
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


ASTNodePtr Parser::makeLeaf(ASTNodeType type, const Token& tok) { return std::make_shared<ASTNode>(type, tok); }

ASTNodePtr Parser::makeNode(ASTNodeType type, const Token& tok, std::initializer_list<ASTNodePtr> children) {
    auto n = std::make_shared<ASTNode>(type, tok);
    n->children.insert(n->children.end(), children.begin(), children.end());
    return n;
}

ASTNodePtr Parser::parseProgram() {
    auto startTok = peek();
    auto root = std::make_shared<ASTNode>(ASTNodeType::Program, startTok);

    if (!isAtEnd()) {
        try { root->children.push_back(parseDeclOrStmtList()); }
        catch (const ParseException&) { synchronize(); }
    }

    if (peek().type != TokenType::END_OF_FILE) {
        try { error(peek(), "Expected end of file"); }
        catch (const ParseException&) {}
    }

    return root;
}

ASTNodePtr Parser::parseProgramInternal() { return parseProgram(); }

ASTNodePtr Parser::parseDeclOrStmtList() {
    auto listNode = std::make_shared<ASTNode>(ASTNodeType::StmtList, peek());
    while (!isAtEnd() && !check(TokenType::RBRACE)) {
        try { listNode->children.push_back(parseDeclOrStmt()); }
        catch (const ParseException&) { synchronize(); }
    }
    return listNode;
}

ASTNodePtr Parser::parseDeclOrStmt() {
    if (check(TokenType::KW_VAR)) return parseVarDecl();
    if (check(TokenType::KW_ARRAY)) return parseArrayDecl();
    if (check(TokenType::KW_INT) || check(TokenType::KW_FLOAT) || check(TokenType::KW_BOOL)) return parseTypedVarDecl();
    return parseStmt();
}

ASTNodePtr Parser::parseStmt() {
    if (isAssignOpKw(peek().type)) return parseAssignOpStmt();
    if (check(TokenType::KW_PRINT)) return parsePrintStmt();
    if (check(TokenType::KW_MATCH)) return parseMatchStmt();
    if (check(TokenType::KW_IF)) return parseIfStmt();
    if (check(TokenType::KW_WHILE)) return parseWhileStmt();
    if (check(TokenType::KW_FOR)) return parseForStmt();
    if (check(TokenType::KW_FOREACH)) return parseForeachStmt();
    if (check(TokenType::LBRACE)) return parseBlock();
    error(peek(), "Unexpected token in statement");
    return nullptr;
}

ASTNodePtr Parser::parseType() {
    if (matchAny({TokenType::KW_INT, TokenType::KW_FLOAT, TokenType::KW_BOOL})) {
        return makeLeaf(ASTNodeType::Identifier, previous());
    }
    error(peek(), "Expected type (int, float, bool)");
    return nullptr;
}

ASTNodePtr Parser::parseVarDecl() {
    Token startTok = expect(TokenType::KW_VAR, "Expected 'var'");
    auto node = std::make_shared<ASTNode>(ASTNodeType::VarDecl, startTok);
    node->children.push_back(parseVarDeclList());
    expect(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

ASTNodePtr Parser::parseVarDeclList() {
    auto listNode = std::make_shared<ASTNode>(ASTNodeType::VarDeclList, peek());
    listNode->children.push_back(parseVarDeclItem());
    while (match(TokenType::COMMA)) listNode->children.push_back(parseVarDeclItem());
    return listNode;
}

ASTNodePtr Parser::parseVarDeclItem() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier in variable declaration");
    auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);
    auto typeNode = parseType();

    auto item = std::make_shared<ASTNode>(ASTNodeType::VarDeclItem, idTok);
    item->children.push_back(idNode);
    item->children.push_back(typeNode);

    if (match(TokenType::ASSIGN)) item->children.push_back(parseExpr());
    return item;
}

ASTNodePtr Parser::parseTypedVarDecl() {
    Token typeTok = peek();
    auto node = std::make_shared<ASTNode>(ASTNodeType::TypedVarDecl, typeTok);
    node->children.push_back(parseType());
    node->children.push_back(parseTypedVarDeclList());
    expect(TokenType::SEMICOLON, "Expected ';' after declaration");
    return node;
}

ASTNodePtr Parser::parseTypedVarDeclList() {
    auto list = std::make_shared<ASTNode>(ASTNodeType::VarDeclList, peek());
    list->children.push_back(parseTypedVarDeclItem());
    while (match(TokenType::COMMA)) list->children.push_back(parseTypedVarDeclItem());
    return list;
}

ASTNodePtr Parser::parseTypedVarDeclItem() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    auto item = std::make_shared<ASTNode>(ASTNodeType::TypedVarDeclItem, idTok);
    item->children.push_back(makeLeaf(ASTNodeType::Identifier, idTok));
    if (match(TokenType::ASSIGN)) item->children.push_back(parseExpr());
    return item;
}

ASTNodePtr Parser::parseArrayDecl() {
    Token startTok = expect(TokenType::KW_ARRAY, "Expected 'array'");
    Token idTok = expect(TokenType::IDENTIFIER, "Expected array name");

    auto node = std::make_shared<ASTNode>(ASTNodeType::ArrayDecl, startTok);
    node->children.push_back(makeLeaf(ASTNodeType::Identifier, idTok));

    if (match(TokenType::ASSIGN)) node->children.push_back(parseArrayInit());

    expect(TokenType::SEMICOLON, "Expected ';' after array declaration");
    return node;
}

ASTNodePtr Parser::parseArrayInit() {
    if (!check(TokenType::LBRACKET)) error(peek(), "Expected '[' for array init");

    if (check(TokenType::LBRACKET) && peek(1).type == TokenType::RBRACKET) return parseArrayLiteral();

    if (check(TokenType::LBRACKET)) {
        std::size_t save = current;
        advance();
        bool comp = false;
        int depth = 1;
        while (!isAtEnd() && depth > 0) {
            if (peek().type == TokenType::LBRACKET) depth++;
            else if (peek().type == TokenType::RBRACKET) depth--;
            else if (peek().type == TokenType::KW_FOR) { comp = true; break; }
            advance();
        }
        current = save;
        if (comp) return parseArrayComp();
        return parseArrayLiteral();
    }

    error(peek(), "Expected array init");
    return nullptr;
}

ASTNodePtr Parser::parseArrayLiteral() {
    Token startTok = expect(TokenType::LBRACKET, "Expected '['");
    auto node = std::make_shared<ASTNode>(ASTNodeType::ArrayLiteral, startTok);
    if (!check(TokenType::RBRACKET)) node->children.push_back(parseExprList());
    expect(TokenType::RBRACKET, "Expected ']'");
    return node;
}

ASTNodePtr Parser::parseArrayComp() {
    Token startTok = expect(TokenType::LBRACKET, "Expected '['");
    auto node = std::make_shared<ASTNode>(ASTNodeType::ArrayComp, startTok);

    node->children.push_back(parseExpr());
    expect(TokenType::KW_FOR, "Expected 'for' in comprehension");
    Token itTok = expect(TokenType::IDENTIFIER, "Expected iterator identifier");
    node->children.push_back(makeLeaf(ASTNodeType::Identifier, itTok));
    expect(TokenType::KW_IN, "Expected 'in' in comprehension");
    Token srcTok = expect(TokenType::IDENTIFIER, "Expected source identifier");
    node->children.push_back(makeLeaf(ASTNodeType::Identifier, srcTok));

    if (match(TokenType::KW_IF)) node->children.push_back(parseCondExpr());

    expect(TokenType::RBRACKET, "Expected ']'");
    return node;
}

ASTNodePtr Parser::parseExprList() {
    auto list = std::make_shared<ASTNode>(ASTNodeType::ExprList, peek());
    list->children.push_back(parseExpr());
    while (match(TokenType::COMMA)) list->children.push_back(parseExpr());
    return list;
}

ASTNodePtr Parser::parseLValue() {
    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    auto base = makeLeaf(ASTNodeType::Identifier, idTok);

    if (match(TokenType::LBRACKET)) {
        auto idx = parseExpr();
        expect(TokenType::RBRACKET, "Expected ']'");
        auto lv = std::make_shared<ASTNode>(ASTNodeType::LValue, idTok);
        lv->children.push_back(base);
        lv->children.push_back(idx);
        return lv;
    }

    return makeNode(ASTNodeType::LValue, idTok, {base});
}

ASTNodePtr Parser::parseAssignOpStmt() {
    Token opTok = peek();
    if (!isAssignOpKw(opTok.type)) error(peek(), "Expected assignment operation");
    advance();

    auto node = std::make_shared<ASTNode>(ASTNodeType::AssignOpStmt, opTok);
    node->children.push_back(parseLValue());

    if (isOneSrcOp(opTok.type)) {
        expect(TokenType::SEMICOLON, "Expected ';' after operation");
        return node;
    }

    node->children.push_back(parseFactor());

    if (isTwoSrcOp(opTok.type)) {
        expect(TokenType::SEMICOLON, "Expected ';' after operation");
        return node;
    }

    node->children.push_back(parseFactor());
    expect(TokenType::SEMICOLON, "Expected ';' after operation");
    return node;
}

ASTNodePtr Parser::parsePrintExpr() {
    Token kw = expect(TokenType::KW_PRINT, "Expected 'print'");
    expect(TokenType::LPAREN, "Expected '(' after 'print'");
    auto exprNode = parseExpr();
    expect(TokenType::RPAREN, "Expected ')' after print argument");

    auto node = std::make_shared<ASTNode>(ASTNodeType::PrintStmt, kw);
    node->children.push_back(exprNode);
    return node;
}

ASTNodePtr Parser::parsePrintStmt() {
    auto node = parsePrintExpr();
    expect(TokenType::SEMICOLON, "Expected ';' after print statement");
    return node;
}

ASTNodePtr Parser::parseMatchStmt() {
    Token kw = expect(TokenType::KW_MATCH, "Expected 'match'");
    auto node = std::make_shared<ASTNode>(ASTNodeType::MatchStmt, kw);

    node->children.push_back(parseExpr());
    expect(TokenType::LBRACE, "Expected '{' after match expr");
    node->children.push_back(parseMatchCaseListOpt());
    expect(TokenType::RBRACE, "Expected '}' after match cases");
    return node;
}

ASTNodePtr Parser::parseMatchCaseListOpt() {
    auto list = std::make_shared<ASTNode>(ASTNodeType::MatchCaseList, peek());
    if (check(TokenType::RBRACE)) return list;

    list->children.push_back(parseMatchCase());
    while (match(TokenType::COMMA)) {
        if (check(TokenType::RBRACE)) break;
        list->children.push_back(parseMatchCase());
    }
    return list;
}

ASTNodePtr Parser::parseMatchCase() {
    auto pat = parseMatchPattern();
    expect(TokenType::ARROW, "Expected '->' in match case");
    auto body = parseMatchBody();
    auto node = std::make_shared<ASTNode>(ASTNodeType::MatchCase, pat->token);
    node->children.push_back(pat);
    node->children.push_back(body);
    return node;
}

ASTNodePtr Parser::parseMatchPattern() {
    if (match(TokenType::INT_LITERAL)) return makeLeaf(ASTNodeType::IntLiteral, previous());
    if (match(TokenType::FLOAT_LITERAL)) return makeLeaf(ASTNodeType::FloatLiteral, previous());
    if (match(TokenType::BOOL_LITERAL)) return makeLeaf(ASTNodeType::BoolLiteral, previous());
    if (match(TokenType::IDENTIFIER)) return makeLeaf(ASTNodeType::Identifier, previous());
    if (match(TokenType::UNDERSCORE)) return makeLeaf(ASTNodeType::Underscore, previous());
    error(peek(), "Expected match pattern");
    return nullptr;
}

ASTNodePtr Parser::parseMatchBody() {
    if (isAssignOpKw(peek().type)) return parseAssignOpStmt();
    if (check(TokenType::KW_PRINT)) return parsePrintExpr();
    if (check(TokenType::LBRACE)) return parseBlock();
    error(peek(), "Expected match body");
    return nullptr;
}

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
        try { node->children.push_back(parseStmt()); }
        catch (const ParseException&) { synchronize(); }
    }
    return node;
}

ASTNodePtr Parser::parseStmtList() { return parseBlockBody(); }

ASTNodePtr Parser::parseIfStmt() {
    Token ifTok = expect(TokenType::KW_IF, "Expected 'if'");
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto cond = parseCondExpr();
    expect(TokenType::RPAREN, "Expected ')'");
    auto thenBlock = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::IfStmt, ifTok);
    node->children.push_back(cond);
    node->children.push_back(thenBlock);

    while (check(TokenType::KW_ELSE) && check(TokenType::KW_IF, 1)) node->children.push_back(parseIfElsePart());
    if (check(TokenType::KW_ELSE)) node->children.push_back(parseIfElsePart());

    return node;
}

ASTNodePtr Parser::parseIfElsePart() {
    Token elseTok = expect(TokenType::KW_ELSE, "Expected 'else'");
    auto node = std::make_shared<ASTNode>(ASTNodeType::IfStmt, elseTok);

    if (match(TokenType::KW_IF)) {
        Token ifTok = previous();
        expect(TokenType::LPAREN, "Expected '(' after 'if'");
        auto cond = parseCondExpr();
        expect(TokenType::RPAREN, "Expected ')'");
        auto blk = parseBlock();
        auto elif = std::make_shared<ASTNode>(ASTNodeType::IfStmt, ifTok);
        elif->children.push_back(cond);
        elif->children.push_back(blk);
        node->children.push_back(elif);
        return node;
    }

    node->children.push_back(parseBlock());
    return node;
}

ASTNodePtr Parser::parseWhileStmt() {
    Token wTok = expect(TokenType::KW_WHILE, "Expected 'while'");
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto cond = parseCondExpr();
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = parseBlock();
    return makeNode(ASTNodeType::WhileStmt, wTok, {cond, body});
}

ASTNodePtr Parser::parseForStmt() {
    Token fTok = expect(TokenType::KW_FOR, "Expected 'for'");
    expect(TokenType::LPAREN, "Expected '(' after 'for'");
    auto init = parseForInit();
    expect(TokenType::SEMICOLON, "Expected ';' after for init");
    auto cond = parseForCond();
    expect(TokenType::SEMICOLON, "Expected ';' after for condition");
    auto upd = parseForUpdate();
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForStmt, fTok);
    node->children.push_back(init);
    node->children.push_back(cond);
    node->children.push_back(upd);
    node->children.push_back(body);
    return node;
}

ASTNodePtr Parser::parseForeachStmt() {
    Token fTok = expect(TokenType::KW_FOREACH, "Expected 'foreach'");
    expect(TokenType::LPAREN, "Expected '('");
    Token varTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    expect(TokenType::KW_IN, "Expected 'in'");
    Token arrTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    expect(TokenType::RPAREN, "Expected ')'");
    auto body = parseBlock();

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForeachStmt, fTok);
    node->children.push_back(makeLeaf(ASTNodeType::Identifier, varTok));
    node->children.push_back(makeLeaf(ASTNodeType::Identifier, arrTok));
    node->children.push_back(body);
    return node;
}

ASTNodePtr Parser::parseForInit() {
    if (check(TokenType::SEMICOLON)) {
        return std::make_shared<ASTNode>(ASTNodeType::ForInit, peek());
    }

    if (match(TokenType::KW_INT)) {
        Token typeTok = previous();
        Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");
        expect(TokenType::ASSIGN, "Expected '='");
        auto expr = parseArithExpr();

        auto node = std::make_shared<ASTNode>(ASTNodeType::ForInit, typeTok);
        node->children.push_back(makeLeaf(ASTNodeType::Identifier, idTok));
        node->children.push_back(expr);
        return node;
    }

    Token idTok = expect(TokenType::IDENTIFIER, "Expected identifier");
    expect(TokenType::ASSIGN, "Expected '='");
    auto expr = parseArithExpr();

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForInit, idTok);
    node->children.push_back(makeLeaf(ASTNodeType::Identifier, idTok));
    node->children.push_back(expr);
    return node;
}

ASTNodePtr Parser::parseForCond() {
    if (check(TokenType::SEMICOLON)) {
        return std::make_shared<ASTNode>(ASTNodeType::ForCond, peek());
    }
    auto c = parseCondExpr();
    auto node = std::make_shared<ASTNode>(ASTNodeType::ForCond, c->token);
    node->children.push_back(c);
    return node;
}

ASTNodePtr Parser::parseForUpdate() {
    if (check(TokenType::RPAREN)) {
        return std::make_shared<ASTNode>(ASTNodeType::ForUpdate, peek());
    }

    if (check(TokenType::IDENTIFIER) && (check(TokenType::PLUS_PLUS, 1) || check(TokenType::MINUS_MINUS, 1))) {
        Token idTok = advance();
        Token opTok = advance();
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
        node->children.push_back(makeLeaf(ASTNodeType::Identifier, idTok));
        node->children.push_back(makeLeaf(ASTNodeType::Identifier, opTok));
        return node;
    }

    if (!isAssignOpKw(peek().type)) error(peek(), "Expected for update");
    Token opTok = advance();

    auto opNode = std::make_shared<ASTNode>(ASTNodeType::AssignOpStmt, opTok);
    opNode->children.push_back(parseLValue());

    if (isOneSrcOp(opTok.type)) {
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
        node->children.push_back(opNode);
        return node;
    }

    opNode->children.push_back(parseFactor());

    if (isTwoSrcOp(opTok.type)) {
        auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
        node->children.push_back(opNode);
        return node;
    }

    opNode->children.push_back(parseFactor());

    auto node = std::make_shared<ASTNode>(ASTNodeType::ForUpdate, opTok);
    node->children.push_back(opNode);
    return node;
}

ASTNodePtr Parser::parseExpr() {
    auto a = parseArithExpr();
    auto node = std::make_shared<ASTNode>(ASTNodeType::Expr, a->token);
    node->children.push_back(a);
    return node;
}

ASTNodePtr Parser::parseCondExpr() {
    auto o = parseBoolOr();
    auto node = std::make_shared<ASTNode>(ASTNodeType::CondExpr, o->token);
    node->children.push_back(o);
    return node;
}

ASTNodePtr Parser::parseBoolOr() {
    auto left = parseBoolAnd();
    while (match(TokenType::LOGIC_OR)) {
        Token opTok = previous();
        auto right = parseBoolAnd();
        auto n = std::make_shared<ASTNode>(ASTNodeType::BoolOr, opTok);
        n->children.push_back(left);
        n->children.push_back(right);
        left = n;
    }
    return left;
}

ASTNodePtr Parser::parseBoolAnd() {
    auto left = parseRelExpr();
    while (match(TokenType::LOGIC_AND)) {
        Token opTok = previous();
        auto right = parseRelExpr();
        auto n = std::make_shared<ASTNode>(ASTNodeType::BoolAnd, opTok);
        n->children.push_back(left);
        n->children.push_back(right);
        left = n;
    }
    return left;
}

ASTNodePtr Parser::parseRelExpr() {
    auto left = parseArithExpr();
    if (matchAny({TokenType::LT, TokenType::GT, TokenType::LTE, TokenType::GTE, TokenType::EQ, TokenType::NEQ})) {
        Token opTok = previous();
        auto right = parseArithExpr();
        auto n = std::make_shared<ASTNode>(ASTNodeType::RelExpr, opTok);
        n->children.push_back(left);
        n->children.push_back(right);
        return n;
    }
    return left;
}

ASTNodePtr Parser::parseArithExpr() {
    auto left = parseTerm();
    while (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        Token opTok = previous();
        auto right = parseTerm();
        auto n = std::make_shared<ASTNode>(ASTNodeType::ArithExpr, opTok);
        n->children.push_back(left);
        n->children.push_back(right);
        left = n;
    }
    return left;
}

ASTNodePtr Parser::parseTerm() {
    auto left = parsePower();
    while (matchAny({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        Token opTok = previous();
        auto right = parsePower();
        auto n = std::make_shared<ASTNode>(ASTNodeType::Term, opTok);
        n->children.push_back(left);
        n->children.push_back(right);
        left = n;
    }
    return left;
}

ASTNodePtr Parser::parsePower() {
    auto base = parseFactor();
    if (match(TokenType::CARET)) {
        Token opTok = previous();
        auto exp = parsePower();
        auto n = std::make_shared<ASTNode>(ASTNodeType::Power, opTok);
        n->children.push_back(base);
        n->children.push_back(exp);
        return n;
    }
    return base;
}

ASTNodePtr Parser::parseFactor() {
    if (match(TokenType::BOOL_LITERAL)) return makeLeaf(ASTNodeType::BoolLiteral, previous());
    if (match(TokenType::INT_LITERAL)) return makeLeaf(ASTNodeType::IntLiteral, previous());
    if (match(TokenType::FLOAT_LITERAL)) return makeLeaf(ASTNodeType::FloatLiteral, previous());
    if (match(TokenType::STRING_LITERAL)) return makeLeaf(ASTNodeType::StringLiteral, previous());

    if (match(TokenType::LPAREN)) {
        auto e = parseCondExpr();
        expect(TokenType::RPAREN, "Expected ')'");
        return e;
    }

    if (matchAny({TokenType::PLUS, TokenType::MINUS})) {
        Token opTok = previous();
        auto rhs = parseFactor();
        auto n = std::make_shared<ASTNode>(ASTNodeType::Factor, opTok);
        n->children.push_back(rhs);
        return n;
    }

    if (check(TokenType::KW_TO_INT) || check(TokenType::KW_TO_FLOAT) || check(TokenType::KW_TO_BOOL) ||
        check(TokenType::KW_ABS) || check(TokenType::KW_LENGTH) || check(TokenType::KW_MAX) ||
        check(TokenType::KW_INDEX) || check(TokenType::KW_FIND)) {
        return parseBuiltinCall();
    }

    if (match(TokenType::IDENTIFIER)) {
        Token idTok = previous();
        auto idNode = makeLeaf(ASTNodeType::Identifier, idTok);

        if (match(TokenType::LBRACKET)) {
            auto idx = parseExpr();
            expect(TokenType::RBRACKET, "Expected ']'");
            auto n = std::make_shared<ASTNode>(ASTNodeType::Factor, idTok);
            n->children.push_back(idNode);
            n->children.push_back(idx);
            return n;
        }

        return idNode;
    }

    error(peek(), "Expected factor");
    return nullptr;
}

ASTNodePtr Parser::parseBuiltinCall() {
    auto name = parseBuiltinName();
    expect(TokenType::LPAREN, "Expected '(' after builtin");

    auto node = std::make_shared<ASTNode>(ASTNodeType::BuiltinCall, name->token);
    if (!check(TokenType::RPAREN)) node->children.push_back(parseArgList());

    expect(TokenType::RPAREN, "Expected ')'");
    return node;
}

ASTNodePtr Parser::parseBuiltinName() {
    if (matchAny({TokenType::KW_TO_INT, TokenType::KW_TO_FLOAT, TokenType::KW_TO_BOOL, TokenType::KW_ABS,
                  TokenType::KW_LENGTH, TokenType::KW_MAX, TokenType::KW_INDEX, TokenType::KW_FIND})) {
        return makeLeaf(ASTNodeType::Identifier, previous());
    }
    error(peek(), "Expected builtin name");
    return nullptr;
}

ASTNodePtr Parser::parseArgList() {
    auto node = std::make_shared<ASTNode>(ASTNodeType::ArgList, peek());
    node->children.push_back(parseExpr());
    while (match(TokenType::COMMA)) node->children.push_back(parseExpr());
    return node;
}
