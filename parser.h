// parser.h
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

#include "Token.h"

struct ParseError {
    int line;
    int column;
    std::string message;
};

class ParseException : public std::runtime_error {
public:
    explicit ParseException(const std::string& msg)
        : std::runtime_error(msg) {}
};

enum class ASTNodeType {
    Program,
    StmtList,
    Block,

    VarDecl,
    VarDeclItem,
    ArrayDecl,
    ArrayLiteral,

    AssignOpStmt,   // ADD / SUB / MUL / ...
    PrintStmt,
    IfStmt,
    WhileStmt,
    ForStmt,
    ForeachStmt,

    ForInit,
    ForCond,
    ForUpdate,

    Expr,
    BoolExpr,
    BoolOr,
    BoolAnd,
    RelExpr,
    ArithExpr,
    Term,
    Power,
    Factor,

    BuiltinCall,
    ArgList,

    Identifier,
    IntLiteral,
    FloatLiteral,
    BoolLiteral,
};

struct ASTNode {
    ASTNodeType type;
    Token token;
    std::vector<std::shared_ptr<ASTNode>> children;

    ASTNode(ASTNodeType t, const Token& tok)
        : type(t), token(tok) {}
};

using ASTNodePtr = std::shared_ptr<ASTNode>;

class Parser {
public:
    Parser(const std::vector<Token>& tokens);

    ASTNodePtr parseProgram();

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<ParseError>& getErrors() const { return errors; }

private:
    const std::vector<Token>& tokens;
    std::size_t current;

    std::vector<ParseError> errors;
    bool panicMode = false;

    // Helpers
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

    // Program
    ASTNodePtr parseProgramInternal();
    ASTNodePtr parseDeclOrStmtList();
    ASTNodePtr parseDeclOrStmt();
    ASTNodePtr parseStmt();

    // Declarations
    ASTNodePtr parseVarDecl();
    ASTNodePtr parseArrayDecl();
    ASTNodePtr parseVarDeclList();
    ASTNodePtr parseVarDeclItem();
    ASTNodePtr parseType();
    ASTNodePtr parseArrayLiteral();
    ASTNodePtr parseExprList();

    // Statements
    ASTNodePtr parseAssignOpStmt();   // OP dst [src] [src]
    ASTNodePtr parseAssignOpDst();    // IDENT or IDENT '[' Expr ']'
    ASTNodePtr parseAssignOpSrc();    // Factor
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

    // Expressions
    ASTNodePtr parseExpr();
    ASTNodePtr parseBoolExpr();
    ASTNodePtr parseBoolOr();
    ASTNodePtr parseBoolAnd();
    ASTNodePtr parseRelOrArith();

    ASTNodePtr parseArithExpr();
    ASTNodePtr parseTerm();
    ASTNodePtr parsePower();
    ASTNodePtr parseFactor();

    // Builtin
    ASTNodePtr parseBuiltinCall();
    ASTNodePtr parseBuiltinName();
    ASTNodePtr parseArgList();
};
