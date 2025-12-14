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
    std::size_t current;
    int line;
    int column;

    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();

    bool skipWhitespaceAndComments(Token& outError);

    Token makeToken(TokenType type, std::size_t start, std::size_t end,
                    int startLine, int startColumn);

    Token makeInvalidToken(const std::string& message,
                           int startLine, int startColumn);

    Token identifierOrKeyword(char firstChar,
                              std::size_t startIndex,
                              int startLine, int startColumn);

    Token numberLiteral(char firstChar,
                        std::size_t startIndex,
                        int startLine, int startColumn);

    TokenType keywordType(const std::string& ident) const;

    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);
};

#endif // LEXER_H
