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

    // ===== Assignment-style statements (Phase1 spec) =====
    KW_ADD,        // ADD x y z
    KW_SUB,        // SUB x y z
    KW_MUL,        // MUL x y z
    KW_DIV,        // DIV x y z
    KW_MOD,        // MOD x y z
    KW_INC,        // INC x
    KW_DEC,        // DEC x
    KW_PLE,        // PLE x y
    KW_MIE,        // MIE x y
    KW_AND,        // AND x y z
    KW_OR,         // OR x y z

    // ===== Literals & identifiers =====
    IDENTIFIER,
    INT_LITERAL,
    FLOAT_LITERAL,
    BOOL_LITERAL,  // true / false

    // ===== Operators (expressions / conditions) =====
    PLUS,          // +
    MINUS,         // -
    STAR,          // *
    SLASH,         // /
    PERCENT,       // %
    CARET,         // ^

    PLUS_PLUS,     // ++
    MINUS_MINUS,   // --

    ASSIGN,        // =   (used in var init / for init)

    // ===== Relational =====
    EQ,            // ==
    NEQ,           // !=
    GT,            // >
    LT,            // <
    GTE,           // >=
    LTE,           // <=

    // ===== Logical =====
    LOGIC_AND,     // &&
    LOGIC_OR,      // ||

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
        case TokenType::KW_OR:  return "KW_OR";

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

        case TokenType::ASSIGN: return "ASSIGN";

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

        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::INVALID: return "INVALID";
        default: return "UNKNOWN";
    }
}

#endif // TOKEN_H
