#ifndef TOKEN_H
#define TOKEN_H

#include <string>

enum class TokenType {
    // ===== Keywords =====
    KW_VAR,        // var
    KW_INT,        // int
    KW_FLOAT,      // float
    KW_BOOL,       // bool
    KW_ARRAY,      // array

    KW_IF,         // if
    KW_ELSE,       // else
    KW_WHILE,      // while
    KW_FOR,        // for
    KW_FOREACH,    // foreach
    KW_IN,         // in

    KW_PRINT,      // print

    // Built-in functions
    KW_TO_INT,     // to_int
    KW_TO_FLOAT,   // to_float
    KW_TO_BOOL,    // to_bool
    KW_ABS,        // abs
    KW_LENGTH,     // length
    KW_MAX,        // max
    KW_INDEX,      // index
    KW_FIND,       // find

    // ===== Literals & identifiers =====
    IDENTIFIER,
    INT_LITERAL,
    FLOAT_LITERAL,
    BOOL_LITERAL,  // true / false

    // ===== Operators (arithmetic) =====
    PLUS,          // +
    MINUS,         // -
    STAR,          // *
    SLASH,         // /
    PERCENT,       // %
    CARET,         // ^

    PLUS_PLUS,     // ++
    MINUS_MINUS,   // --

    PLUS_EQ,       // +=
    MINUS_EQ,      // -=
    STAR_EQ,       // *=
    SLASH_EQ,      // /=
    PERCENT_EQ,    // %=

    ASSIGN,        // =

    // ===== Relational =====
    EQ,            // ==
    NEQ,           // !=
    GT,            // >
    LT,            // <
    GTE,           // >=
    LTE,           // <=

    // ===== Logical / bitwise =====
    LOGIC_AND,     // &&
    LOGIC_OR,      // ||
    BIT_AND,       // &
    BIT_OR,        // |

    // ===== Punctuation =====
    LPAREN,        // (
    RPAREN,        // )
    LBRACE,        // {
    RBRACE,        // }
    LBRACKET,      // [
    RBRACKET,      // ]
    COMMA,         // ,
    SEMICOLON,     // ;

    // ===== Special =====
    END_OF_FILE,
    INVALID        // for lexing errors (unknown character, etc.)
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column; // starting column of this token (1-based)

    Token() : type(TokenType::INVALID), lexeme(""), line(0), column(0) {}

    Token(TokenType t, const std::string& lex, int l, int c)
        : type(t), lexeme(lex), line(l), column(c) {}
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
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::BOOL_LITERAL: return "BOOL_LITERAL";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::CARET: return "CARET";
        case TokenType::PLUS_PLUS: return "PLUS_PLUS";
        case TokenType::MINUS_MINUS: return "MINUS_MINUS";
        case TokenType::PLUS_EQ: return "PLUS_EQ";
        case TokenType::MINUS_EQ: return "MINUS_EQ";
        case TokenType::STAR_EQ: return "STAR_EQ";
        case TokenType::SLASH_EQ: return "SLASH_EQ";
        case TokenType::PERCENT_EQ: return "PERCENT_EQ";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::EQ: return "EQ";
        case TokenType::NEQ: return "NEQ";
        case TokenType::GT: return "GT";
        case TokenType::LT: return "LT";
        case TokenType::GTE: return "GTE";
        case TokenType::LTE: return "LTE";
        case TokenType::LOGIC_AND: return "LOGIC_AND";
        case TokenType::LOGIC_OR: return "LOGIC_OR";
        case TokenType::BIT_AND: return "BIT_AND";
        case TokenType::BIT_OR: return "BIT_OR";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

#endif // TOKEN_H
