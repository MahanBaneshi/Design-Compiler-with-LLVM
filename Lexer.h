#ifndef LEXER_H
#define LEXER_H

#include <string>
#include <vector>
#include "Token.h"

class Lexer {
public:
    explicit Lexer(const std::string& source);

    Token nextToken();
    std::vector<Token> tokenizeAll();

private:
    std::string source;
    std::size_t current = 0;
    int line = 1;
    int column = 1;

    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);

    bool skipWhitespaceAndComments(Token& outError);

    Token makeToken(TokenType type, std::size_t start, std::size_t end,
                    int startLine, int startColumn);
    Token makeInvalidToken(const std::string& message, int startLine, int startColumn);

    Token identifierOrKeyword(std::size_t startIndex, int startLine, int startColumn);
    Token numberLiteral(std::size_t startIndex, int startLine, int startColumn);
    Token stringLiteral(int startLine, int startColumn);

    Token twoCharToken(TokenType one, TokenType two, char expectedSecond,
                       std::size_t startIndex, int startLine, int startColumn);
    Token arrowTokenOrMinus(std::size_t startIndex, int startLine, int startColumn);

    TokenType keywordType(const std::string& ident) const;

    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);
    static bool isDigit(char c);

    static bool isIdentStart(char c);
    static bool isIdentBody(char c);
};

#endif
