#include "Lexer.h"
#include <cctype>

Lexer::Lexer(const std::string& src) : source(src), current(0), line(1), column(1) {}

bool Lexer::isAtEnd() const { return current >= source.size(); }

char Lexer::peek() const { return isAtEnd() ? '\0' : source[current]; }

char Lexer::peekNext() const { return (current + 1 >= source.size()) ? '\0' : source[current + 1]; }

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = source[current++];
    if (c == '\n') { ++line; column = 1; }
    else { ++column; }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAlpha(char c) { return std::isalpha(static_cast<unsigned char>(c)) != 0; }

bool Lexer::isDigit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

bool Lexer::isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c) || c == '_'; }

bool Lexer::isIdentStart(char c) { return isAlpha(c); }

bool Lexer::isIdentBody(char c) { return isAlpha(c) || isDigit(c) || c == '_'; }

bool Lexer::skipWhitespaceAndComments(Token& outError) {
    while (!isAtEnd()) {
        char c = peek();

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v') {
            advance();
            continue;
        }

        if (c == '/' && peekNext() == '*') {
            int startLine = line;
            int startColumn = column;

            advance();
            advance();

            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }

            if (!closed) {
                outError = makeInvalidToken("Unterminated comment", startLine, startColumn);
                return false;
            }

            continue;
        }

        break;
    }

    return true;
}

Token Lexer::makeToken(TokenType type, std::size_t start, std::size_t end, int startLine, int startColumn) {
    return Token(type, source.substr(start, end - start), startLine, startColumn);
}

Token Lexer::makeInvalidToken(const std::string& message, int startLine, int startColumn) {
    return Token(TokenType::INVALID, message, startLine, startColumn);
}

TokenType Lexer::keywordType(const std::string& ident) const {
    if (ident == "var")      return TokenType::KW_VAR;
    if (ident == "int")      return TokenType::KW_INT;
    if (ident == "float")    return TokenType::KW_FLOAT;
    if (ident == "bool")     return TokenType::KW_BOOL;
    if (ident == "array")    return TokenType::KW_ARRAY;

    if (ident == "if")       return TokenType::KW_IF;
    if (ident == "else")     return TokenType::KW_ELSE;
    if (ident == "while")    return TokenType::KW_WHILE;
    if (ident == "for")      return TokenType::KW_FOR;
    if (ident == "foreach")  return TokenType::KW_FOREACH;
    if (ident == "in")       return TokenType::KW_IN;
    if (ident == "match")    return TokenType::KW_MATCH;

    if (ident == "print")    return TokenType::KW_PRINT;

    if (ident == "to_int")   return TokenType::KW_TO_INT;
    if (ident == "to_float") return TokenType::KW_TO_FLOAT;
    if (ident == "to_bool")  return TokenType::KW_TO_BOOL;
    if (ident == "abs")      return TokenType::KW_ABS;
    if (ident == "length")   return TokenType::KW_LENGTH;
    if (ident == "max")      return TokenType::KW_MAX;
    if (ident == "index")    return TokenType::KW_INDEX;
    if (ident == "find")     return TokenType::KW_FIND;

    if (ident == "ADD")      return TokenType::KW_ADD;
    if (ident == "SUB")      return TokenType::KW_SUB;
    if (ident == "MUL")      return TokenType::KW_MUL;
    if (ident == "DIV")      return TokenType::KW_DIV;
    if (ident == "MOD")      return TokenType::KW_MOD;
    if (ident == "INC")      return TokenType::KW_INC;
    if (ident == "DEC")      return TokenType::KW_DEC;
    if (ident == "PLE")      return TokenType::KW_PLE;
    if (ident == "MIE")      return TokenType::KW_MIE;
    if (ident == "AND")      return TokenType::KW_AND;
    if (ident == "OR")       return TokenType::KW_OR;

    if (ident == "true" || ident == "false") return TokenType::BOOL_LITERAL;

    return TokenType::IDENTIFIER;
}

Token Lexer::identifierOrKeyword(std::size_t startIndex, int startLine, int startColumn) {
    while (!isAtEnd() && isIdentBody(peek())) advance();
    std::size_t endIndex = current;
    std::string text = source.substr(startIndex, endIndex - startIndex);
    return Token(keywordType(text), text, startLine, startColumn);
}

Token Lexer::numberLiteral(std::size_t startIndex, int startLine, int startColumn) {
    bool isFloat = false;

    while (!isAtEnd() && isDigit(peek())) advance();

    if (!isAtEnd() && peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance();
        while (!isAtEnd() && isDigit(peek())) advance();
    }

    std::size_t endIndex = current;
    std::string text = source.substr(startIndex, endIndex - startIndex);
    return Token(isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL, text, startLine, startColumn);
}

Token Lexer::stringLiteral(int startLine, int startColumn) {
    std::size_t startIndex = current - 1;

    while (!isAtEnd()) {
        char c = advance();
        if (c == '"') {
            return makeToken(TokenType::STRING_LITERAL, startIndex, current, startLine, startColumn);
        }
        if (c == '\\') {
            if (!isAtEnd()) advance();
            continue;
        }
        if (c == '\n') {
            return makeInvalidToken("Unterminated string literal", startLine, startColumn);
        }
    }

    return makeInvalidToken("Unterminated string literal", startLine, startColumn);
}

Token Lexer::twoCharToken(TokenType one, TokenType two, char expectedSecond,
                          std::size_t startIndex, int startLine, int startColumn) {
    if (match(expectedSecond)) return makeToken(two, startIndex, current, startLine, startColumn);
    return makeToken(one, startIndex, current, startLine, startColumn);
}

Token Lexer::arrowTokenOrMinus(std::size_t startIndex, int startLine, int startColumn) {
    if (match('>')) return makeToken(TokenType::ARROW, startIndex, current, startLine, startColumn);
    if (match('-')) return makeToken(TokenType::MINUS_MINUS, startIndex, current, startLine, startColumn);
    return makeToken(TokenType::MINUS, startIndex, current, startLine, startColumn);
}

Token Lexer::nextToken() {
    Token err;
    if (!skipWhitespaceAndComments(err)) return err;

    if (isAtEnd()) return Token(TokenType::END_OF_FILE, "", line, column);

    int startLine = line;
    int startColumn = column;
    std::size_t startIndex = current;

    char c = advance();

    if (isIdentStart(c)) return identifierOrKeyword(startIndex, startLine, startColumn);
    if (isDigit(c)) return numberLiteral(startIndex, startLine, startColumn);

    if (c == '_') return makeToken(TokenType::UNDERSCORE, startIndex, current, startLine, startColumn);
    if (c == '"') return stringLiteral(startLine, startColumn);

    switch (c) {
        case '(': return makeToken(TokenType::LPAREN, startIndex, current, startLine, startColumn);
        case ')': return makeToken(TokenType::RPAREN, startIndex, current, startLine, startColumn);
        case '{': return makeToken(TokenType::LBRACE, startIndex, current, startLine, startColumn);
        case '}': return makeToken(TokenType::RBRACE, startIndex, current, startLine, startColumn);
        case '[': return makeToken(TokenType::LBRACKET, startIndex, current, startLine, startColumn);
        case ']': return makeToken(TokenType::RBRACKET, startIndex, current, startLine, startColumn);
        case ';': return makeToken(TokenType::SEMICOLON, startIndex, current, startLine, startColumn);
        case ',': return makeToken(TokenType::COMMA, startIndex, current, startLine, startColumn);

        case '+': return twoCharToken(TokenType::PLUS, TokenType::PLUS_PLUS, '+', startIndex, startLine, startColumn);
        case '*': return makeToken(TokenType::STAR, startIndex, current, startLine, startColumn);
        case '/': return makeToken(TokenType::SLASH, startIndex, current, startLine, startColumn);
        case '%': return makeToken(TokenType::PERCENT, startIndex, current, startLine, startColumn);
        case '^': return makeToken(TokenType::CARET, startIndex, current, startLine, startColumn);

        case '-': return arrowTokenOrMinus(startIndex, startLine, startColumn);

        case '=': return twoCharToken(TokenType::ASSIGN, TokenType::EQ, '=', startIndex, startLine, startColumn);

        case '!':
            if (match('=')) return makeToken(TokenType::NEQ, startIndex, current, startLine, startColumn);
            return makeInvalidToken("Unexpected '!'", startLine, startColumn);

        case '<': return twoCharToken(TokenType::LT, TokenType::LTE, '=', startIndex, startLine, startColumn);
        case '>': return twoCharToken(TokenType::GT, TokenType::GTE, '=', startIndex, startLine, startColumn);

        case '&':
            if (match('&')) return makeToken(TokenType::LOGIC_AND, startIndex, current, startLine, startColumn);
            return makeInvalidToken("Unexpected '&'", startLine, startColumn);

        case '|':
            if (match('|')) return makeToken(TokenType::LOGIC_OR, startIndex, current, startLine, startColumn);
            return makeInvalidToken("Unexpected '|'", startLine, startColumn);

        default:
            return makeInvalidToken(std::string("Unexpected character: '") + c + "'", startLine, startColumn);
    }
}

std::vector<Token> Lexer::tokenizeAll() {
    std::vector<Token> tokens;
    while (true) {
        Token t = nextToken();
        tokens.push_back(t);
        if (t.type == TokenType::END_OF_FILE) break;
    }
    return tokens;
}
