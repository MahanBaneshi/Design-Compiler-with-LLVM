// Lexer.cpp
#include "Lexer.h"
#include <cctype>
#include <stdexcept>

Lexer::Lexer(const std::string& src)
    : source(src), current(0), line(1), column(1) {}

bool Lexer::isAtEnd() const {
    return current >= source.size();
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.size()) return '\0';
    return source[current + 1];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = source[current++];
    if (c == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
    return c;
}

bool Lexer::isAlpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || std::isdigit(static_cast<unsigned char>(c));
}

bool Lexer::skipWhitespaceAndComments(Token& outError) {
    while (!isAtEnd()) {
        char c = peek();

        // whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '\f' || c == '\v') {
            advance();
            continue;
        }

        // Comment: /* ... */
        if (c == '/' && peekNext() == '*') {
            int startLine = line;
            int startColumn = column;

            advance(); // '/'
            advance(); // '*'

            bool closed = false;
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // '*'
                    advance(); // '/'
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

Token Lexer::makeToken(TokenType type, std::size_t start, std::size_t end,
                       int startLine, int startColumn) {
    std::string lex = source.substr(start, end - start);
    return Token(type, lex, startLine, startColumn);
}

Token Lexer::makeInvalidToken(const std::string& message,
                              int startLine, int startColumn) {
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

    if (ident == "print")    return TokenType::KW_PRINT;

    if (ident == "to_int")   return TokenType::KW_TO_INT;
    if (ident == "to_float") return TokenType::KW_TO_FLOAT;
    if (ident == "to_bool")  return TokenType::KW_TO_BOOL;
    if (ident == "abs")      return TokenType::KW_ABS;
    if (ident == "length")   return TokenType::KW_LENGTH;
    if (ident == "max")      return TokenType::KW_MAX;
    if (ident == "index")    return TokenType::KW_INDEX;
    if (ident == "find")     return TokenType::KW_FIND;

    // Phase1 assignment-style statements (UPPERCASE)
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

    if (ident == "true" || ident == "false")
        return TokenType::BOOL_LITERAL;

    return TokenType::IDENTIFIER;
}

Token Lexer::identifierOrKeyword(char firstChar,
                                 std::size_t startIndex,
                                 int startLine, int startColumn) {
    (void)firstChar;
    while (!isAtEnd() && isAlphaNumeric(peek())) {
        advance();
    }
    std::size_t endIndex = current;
    std::string text = source.substr(startIndex, endIndex - startIndex);
    TokenType type = keywordType(text);
    return Token(type, text, startLine, startColumn);
}

Token Lexer::numberLiteral(char firstChar,
                           std::size_t startIndex,
                           int startLine, int startColumn) {
    (void)firstChar;

    bool isFloat = false;

    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    if (!isAtEnd() && peek() == '.' &&
        std::isdigit(static_cast<unsigned char>(peekNext()))) {
        isFloat = true;
        advance(); // '.'
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    std::size_t endIndex = current;
    std::string text = source.substr(startIndex, endIndex - startIndex);
    if (isFloat) {
        return Token(TokenType::FLOAT_LITERAL, text, startLine, startColumn);
    } else {
        return Token(TokenType::INT_LITERAL, text, startLine, startColumn);
    }
}

Token Lexer::nextToken() {
    Token err;
    if (!skipWhitespaceAndComments(err)) {
        return err;
    }

    if (isAtEnd()) {
        return Token(TokenType::END_OF_FILE, "", line, column);
    }

    int startLine = line;
    int startColumn = column;
    std::size_t startIndex = current;

    char c = advance();

    if (isAlpha(c)) {
        return identifierOrKeyword(c, startIndex, startLine, startColumn);
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
        return numberLiteral(c, startIndex, startLine, startColumn);
    }

    switch (c) {
        case '(':
            return makeToken(TokenType::LPAREN, startIndex, current, startLine, startColumn);
        case ')':
            return makeToken(TokenType::RPAREN, startIndex, current, startLine, startColumn);
        case '{':
            return makeToken(TokenType::LBRACE, startIndex, current, startLine, startColumn);
        case '}':
            return makeToken(TokenType::RBRACE, startIndex, current, startLine, startColumn);
        case '[':
            return makeToken(TokenType::LBRACKET, startIndex, current, startLine, startColumn);
        case ']':
            return makeToken(TokenType::RBRACKET, startIndex, current, startLine, startColumn);
        case ';':
            return makeToken(TokenType::SEMICOLON, startIndex, current, startLine, startColumn);
        case ',':
            return makeToken(TokenType::COMMA, startIndex, current, startLine, startColumn);

        case '+':
            return makeToken(TokenType::PLUS, startIndex, current, startLine, startColumn);
        case '-':
            return makeToken(TokenType::MINUS, startIndex, current, startLine, startColumn);
        case '*':
            return makeToken(TokenType::STAR, startIndex, current, startLine, startColumn);
        case '/':
            return makeToken(TokenType::SLASH, startIndex, current, startLine, startColumn);
        case '%':
            return makeToken(TokenType::PERCENT, startIndex, current, startLine, startColumn);
        case '^':
            return makeToken(TokenType::CARET, startIndex, current, startLine, startColumn);

        case '=': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::EQ, startIndex, current, startLine, startColumn);
            }
            return makeToken(TokenType::ASSIGN, startIndex, current, startLine, startColumn);
        }

        case '!': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::NEQ, startIndex, current, startLine, startColumn);
            }
            return makeInvalidToken("Unexpected '!'", startLine, startColumn);
        }

        case '<': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::LTE, startIndex, current, startLine, startColumn);
            }
            return makeToken(TokenType::LT, startIndex, current, startLine, startColumn);
        }

        case '>': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::GTE, startIndex, current, startLine, startColumn);
            }
            return makeToken(TokenType::GT, startIndex, current, startLine, startColumn);
        }

        case '&': {
            if (peek() == '&') {
                advance();
                return makeToken(TokenType::LOGIC_AND, startIndex, current, startLine, startColumn);
            }
            return makeInvalidToken("Unexpected '&'", startLine, startColumn);
        }

        case '|': {
            if (peek() == '|') {
                advance();
                return makeToken(TokenType::LOGIC_OR, startIndex, current, startLine, startColumn);
            }
            return makeInvalidToken("Unexpected '|'", startLine, startColumn);
        }

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
