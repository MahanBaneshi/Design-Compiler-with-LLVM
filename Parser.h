#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

#include "token.h"

// =============================
// Error types
// =============================

struct ParseError {
    int line;
    int column;
    std::string message;
};

// استثناء سبک فقط برای کنترل جریان در داخل Parser
class ParseException : public std::runtime_error {
public:
    explicit ParseException(const std::string& msg)
        : std::runtime_error(msg) {}
};

// =============================
// AST node type
// =============================

enum class ASTNodeType {
    // ریشه
    Program,

    // سطح بالا / لیست‌ها
    StmtList,
    Block,

    // تعریف‌ها
    VarDecl,
    VarDeclItem,
    ArrayDecl,
    ArrayLiteral,

    // دستورات
    AssignStmt,
    UnaryStmt,
    PrintStmt,
    IfStmt,
    WhileStmt,
    ForStmt,
    ForeachStmt,

    // اجزای for
    ForInit,
    ForCond,
    ForUpdate,

    // عبارات
    Expr,          // گرهٔ کلی برای Expr
    BoolExpr,      // گرهٔ کلی برای BoolExpr (در عمل Expr است)
    BoolOr,
    BoolAnd,
    RelExpr,
    BitOr,
    BitAnd,
    ArithExpr,
    Term,
    Power,
    Factor,

    // Builtin
    BuiltinCall,
    ArgList,

    // برگ‌ها
    Identifier,
    IntLiteral,
    FloatLiteral,
    BoolLiteral,

    // عملگرها
    UnaryOp,
};

// =============================
// AST node
// =============================

struct ASTNode {
    ASTNodeType type;
    Token token;  // برای نگه داشتن اطلاعات موقعیت/lexeme اصلی نود
    std::vector<std::shared_ptr<ASTNode>> children;

    ASTNode(ASTNodeType t, const Token& tok)
        : type(t), token(tok) {}
};

using ASTNodePtr = std::shared_ptr<ASTNode>;

// =============================
// Parser
// =============================

class Parser {
public:
    Parser(const std::vector<Token>& tokens);

    ASTNodePtr parseProgram();

    // === Error API ===
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<ParseError>& getErrors() const { return errors; }

private:
    const std::vector<Token>& tokens;
    std::size_t current;          // اندیس توکن فعلی

    // Error handling
    std::vector<ParseError> errors;
    bool panicMode = false;

    // کمکی‌ها
    const Token& peek(int offset = 0) const;
    bool isAtEnd() const;
    bool check(TokenType type, int offset = 0) const;
    bool match(TokenType type);
    const Token& advance();
    const Token& previous() const;
    const Token& expect(TokenType type, const std::string& message);

    void error(const Token& token, const std::string& message);
    void synchronize();

    ASTNodePtr makeLeaf(ASTNodeType type, const Token& tok);
    ASTNodePtr makeNode(ASTNodeType type, const Token& tok,
                        std::initializer_list<ASTNodePtr> children);

    // === مطابق گرامر ===

    // Program
    ASTNodePtr parseProgramInternal();
    ASTNodePtr parseDeclOrStmtList();
    ASTNodePtr parseDeclOrStmt();
    ASTNodePtr parseStmt();

    // Decl
    ASTNodePtr parseVarDecl();
    ASTNodePtr parseArrayDecl();
    ASTNodePtr parseVarDeclList();
    ASTNodePtr parseVarDeclItem();
    ASTNodePtr parseType();
    ASTNodePtr parseArrayLiteral();
    ASTNodePtr parseExprList();

    // Statements
    ASTNodePtr parseAssignStmt();
    ASTNodePtr parseUnaryStmt();
    ASTNodePtr parsePrintStmt();
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseIfElsePart();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseForeachStmt();
    ASTNodePtr parseBlock();
    ASTNodePtr parseBlockBody();
    ASTNodePtr parseStmtList();

    // For parts
    ASTNodePtr parseForInit();
    ASTNodePtr parseForCond();
    ASTNodePtr parseForUpdate();
    ASTNodePtr parseUnaryExpr();

    // Expressions (با تقدم)
    ASTNodePtr parseExpr();      // -> BoolExpr
    ASTNodePtr parseBoolExpr();
    ASTNodePtr parseBoolOr();
    ASTNodePtr parseBoolAnd();
    ASTNodePtr parseRelOrArith(); // معادل BoolRel ساده‌شده

    ASTNodePtr parseBitOr();
    ASTNodePtr parseBitAnd();

    ASTNodePtr parseArithExpr();
    ASTNodePtr parseTerm();
    ASTNodePtr parsePower();
    ASTNodePtr parseFactor();

    // Builtin
    ASTNodePtr parseBuiltinCall();
    ASTNodePtr parseBuiltinName();
    ASTNodePtr parseArgList();
};
