#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum class TokenType {
    KW_VAR, KW_INT, KW_FLOAT, KW_BOOL, KW_ARRAY,
    KW_IF, KW_ELSE, KW_WHILE, KW_FOR, KW_FOREACH, KW_IN,
    KW_PRINT,
    KW_TO_INT, KW_TO_FLOAT, KW_TO_BOOL, KW_ABS, KW_LENGTH, KW_MAX, KW_INDEX, KW_FIND,
    KW_ADD, KW_SUB, KW_MUL, KW_DIV, KW_MOD, KW_INC, KW_DEC, KW_PLE, KW_MIE, KW_AND, KW_OR,
    KW_MATCH, KW_CASE, KW_DEFAULT,

    IDENTIFIER, INT_LITERAL, FLOAT_LITERAL, BOOL_LITERAL, STRING_LITERAL,

    PLUS, MINUS, STAR, SLASH, PERCENT, CARET,
    PLUS_PLUS, MINUS_MINUS,
    ASSIGN,
    ARROW,

    EQ, NEQ, GT, LT, GTE, LTE,
    LOGIC_AND, LOGIC_OR,

    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    COMMA, SEMICOLON, COLON,
    UNDERSCORE,

    END_OF_FILE,
    INVALID
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;

    Token() : type(TokenType::INVALID), lexeme(""), line(0), column(0) {}
    Token(TokenType t, const std::string& lex, int l, int c) : type(t), lexeme(lex), line(l), column(c) {}
};

inline const char* tokenTypeToString(TokenType t) {
    switch (t) {
        case TokenType::KW_VAR: return "KW_VAR";
        case TokenType::KW_INT: return "KW_INT";
        case TokenType::KW_FLOAT: return "KW_FLOAT";
        case TokenType::KW_BOOL: return "KW_BOOL";
        case TokenType::KW_ARRAY: return "KW_ARRAY";

        case TokenType::KW_IF: return "KW_IF";
        case TokenType::KW_ELSE: return "KW_ELSE";
        case TokenType::KW_WHILE: return "KW_WHILE";
        case TokenType::KW_FOR: return "KW_FOR";
        case TokenType::KW_FOREACH: return "KW_FOREACH";
        case TokenType::KW_IN: return "KW_IN";

        case TokenType::KW_PRINT: return "KW_PRINT";

        case TokenType::KW_TO_INT: return "KW_TO_INT";
        case TokenType::KW_TO_FLOAT: return "KW_TO_FLOAT";
        case TokenType::KW_TO_BOOL: return "KW_TO_BOOL";
        case TokenType::KW_ABS: return "KW_ABS";
        case TokenType::KW_LENGTH: return "KW_LENGTH";
        case TokenType::KW_MAX: return "KW_MAX";
        case TokenType::KW_INDEX: return "KW_INDEX";
        case TokenType::KW_FIND: return "KW_FIND";

        case TokenType::KW_ADD: return "KW_ADD";
        case TokenType::KW_SUB: return "KW_SUB";
        case TokenType::KW_MUL: return "KW_MUL";
        case TokenType::KW_DIV: return "KW_DIV";
        case TokenType::KW_MOD: return "KW_MOD";
        case TokenType::KW_INC: return "KW_INC";
        case TokenType::KW_DEC: return "KW_DEC";
        case TokenType::KW_PLE: return "KW_PLE";
        case TokenType::KW_MIE: return "KW_MIE";
        case TokenType::KW_AND: return "KW_AND";
        case TokenType::KW_OR: return "KW_OR";

        case TokenType::KW_MATCH: return "KW_MATCH";
        case TokenType::KW_CASE: return "KW_CASE";
        case TokenType::KW_DEFAULT: return "KW_DEFAULT";

        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::BOOL_LITERAL: return "BOOL_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";

        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::CARET: return "CARET";
        case TokenType::PLUS_PLUS: return "PLUS_PLUS";
        case TokenType::MINUS_MINUS: return "MINUS_MINUS";
        case TokenType::ASSIGN: return "ASSIGN";

        case TokenType::ARROW: return "ARROW";

        case TokenType::EQ: return "EQ";
        case TokenType::NEQ: return "NEQ";
        case TokenType::GT: return "GT";
        case TokenType::LT: return "LT";
        case TokenType::GTE: return "GTE";
        case TokenType::LTE: return "LTE";

        case TokenType::LOGIC_AND: return "LOGIC_AND";
        case TokenType::LOGIC_OR: return "LOGIC_OR";

        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COLON: return "COLON";
        case TokenType::UNDERSCORE: return "UNDERSCORE";

        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

#endif
