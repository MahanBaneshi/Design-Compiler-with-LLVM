#include "Lexer.h"
#include <cctype>   // std::isalpha, std::isdigit
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

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
            c == '\f' || c == '\v') {
            advance();
            continue;
        }

        // Comment: /* ... */
        if (c == '/' && peekNext() == '*') {
            // consume '/*'
            advance();
            advance();
            // skip until '*/' or EOF
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance(); // '*'
                    advance(); // '/'
                    break;
                }
                advance();
            }
            // اگر EOF شد و '*/' ندیدیم، فعلاً بی‌صدا ادامه می‌دهیم
            continue;
        }

        // اگر نه whitespace و نه comment بود، می‌شکنیم
        break;
    }
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

    // true/false را به عنوان literal می‌گیریم، نه keyword
    if (ident == "true" || ident == "false") {
        return TokenType::BOOL_LITERAL;
    }

    return TokenType::IDENTIFIER;
}

Token Lexer::identifierOrKeyword(char firstChar,
                                 std::size_t startIndex,
                                 int startLine, int startColumn) {
    // firstChar already consumed
    (void)firstChar; // unused warning
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
    (void)firstChar; // already consumed

    bool isFloat = false;

    // بخش صحیح
    while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    // بخش اعشاری
    if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        isFloat = true;
        advance(); // consume '.'
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
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return Token(TokenType::END_OF_FILE, "", line, column);
    }

    int startLine = line;
    int startColumn = column;
    std::size_t startIndex = current;

    char c = advance();

    // Identifier or keyword
    if (isAlpha(c)) {
        return identifierOrKeyword(c, startIndex, startLine, startColumn);
    }

    // Number literal (int or float)
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return numberLiteral(c, startIndex, startLine, startColumn);
    }

    // Operators & punctuation
    switch (c) {
        // Single-char punctuation
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

        // Operators with possible 2-char variants
        case '+': {
            if (peek() == '+') {
                advance();
                return makeToken(TokenType::PLUS_PLUS, startIndex, current, startLine, startColumn);
            } else if (peek() == '=') {
                advance();
                return makeToken(TokenType::PLUS_EQ, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::PLUS, startIndex, current, startLine, startColumn);
            }
        }
        case '-': {
            if (peek() == '-') {
                advance();
                return makeToken(TokenType::MINUS_MINUS, startIndex, current, startLine, startColumn);
            } else if (peek() == '=') {
                advance();
                return makeToken(TokenType::MINUS_EQ, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::MINUS, startIndex, current, startLine, startColumn);
            }
        }
        case '*': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::STAR_EQ, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::STAR, startIndex, current, startLine, startColumn);
            }
        }
        case '/': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::SLASH_EQ, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::SLASH, startIndex, current, startLine, startColumn);
            }
        }
        case '%': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::PERCENT_EQ, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::PERCENT, startIndex, current, startLine, startColumn);
            }
        }
        case '^':
            return makeToken(TokenType::CARET, startIndex, current, startLine, startColumn);

        case '=': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::EQ, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::ASSIGN, startIndex, current, startLine, startColumn);
            }
        }

        case '!': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::NEQ, startIndex, current, startLine, startColumn);
            } else {
                // زبان ! (نفی) ندارد، پس این خطاست
                return makeInvalidToken("Unexpected '!'", startLine, startColumn);
            }
        }

        case '<': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::LTE, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::LT, startIndex, current, startLine, startColumn);
            }
        }

        case '>': {
            if (peek() == '=') {
                advance();
                return makeToken(TokenType::GTE, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::GT, startIndex, current, startLine, startColumn);
            }
        }

        case '&': {
            if (peek() == '&') {
                advance();
                return makeToken(TokenType::LOGIC_AND, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::BIT_AND, startIndex, current, startLine, startColumn);
            }
        }

        case '|': {
            if (peek() == '|') {
                advance();
                return makeToken(TokenType::LOGIC_OR, startIndex, current, startLine, startColumn);
            } else {
                return makeToken(TokenType::BIT_OR, startIndex, current, startLine, startColumn);
            }
        }

        default:
            // کاراکتر ناشناخته
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
