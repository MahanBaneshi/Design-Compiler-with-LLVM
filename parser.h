#pragma once

#include <vector>
#include <memory>
#include <string>
#include <stdexcept>

#include "Token.h"

// =============================
// Parse error
// =============================

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

// =============================
// AST
// =============================

enum class ASTNodeType {
    Program,
    StmtList,
    Block,

    VarDecl,
    VarDeclList,
    VarDeclItem,

    TypedVarDecl,
    TypedVarDeclItem,

    ArrayDecl,
    ArrayInit,
    ArrayLiteral,
    ArrayComp,
    ExprList,

    AssignOpStmt,
    PrintStmt,

    MatchStmt,
    MatchCaseList,
    MatchCase,
    MatchPattern,
    MatchBody,

    IfStmt,
    WhileStmt,
    ForStmt,
    ForeachStmt,

    ForInit,
    ForCond,
    ForUpdate,

    Expr,
    CondExpr,
    BoolOr,
    BoolAnd,
    RelExpr,
    ArithExpr,
    Term,
    Power,
    Factor,

    BuiltinCall,
    ArgList,

    LValue,

    Identifier,
    IntLiteral,
    FloatLiteral,
    BoolLiteral,
    StringLiteral,
    Underscore
};

struct ASTNode {
    ASTNodeType type;
    Token token;
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
    explicit Parser(const std::vector<Token>& tokens);

    ASTNodePtr parseProgram();

    bool hasErrors() const { return !errors.empty(); }
    const std::vector<ParseError>& getErrors() const { return errors; }

private:
    const std::vector<Token>& tokens;
    std::size_t current = 0;

    std::vector<ParseError> errors;
    bool panicMode = false;

    // helpers
    const Token& peek(int offset = 0) const;
    bool isAtEnd() const;
    bool check(TokenType type, int offset = 0) const;
    bool match(TokenType type);
    bool matchAny(std::initializer_list<TokenType> types);
    const Token& advance();
    const Token& previous() const;
    const Token& expect(TokenType type, const std::string& message);

    void error(const Token& token, const std::string& message);
    void synchronize();

    ASTNodePtr makeLeaf(ASTNodeType type, const Token& tok);
    ASTNodePtr makeNode(ASTNodeType type, const Token& tok,
                        std::initializer_list<ASTNodePtr> children);

    // program
    ASTNodePtr parseProgramInternal();
    ASTNodePtr parseDeclOrStmtList();
    ASTNodePtr parseDeclOrStmt();
    ASTNodePtr parseStmt();

    // declarations
    ASTNodePtr parseType();
    ASTNodePtr parseVarDecl();
    ASTNodePtr parseVarDeclList();
    ASTNodePtr parseVarDeclItem();

    ASTNodePtr parseTypedVarDecl();
    ASTNodePtr parseTypedVarDeclList();
    ASTNodePtr parseTypedVarDeclItem();

    ASTNodePtr parseArrayDecl();
    ASTNodePtr parseArrayInit();
    ASTNodePtr parseArrayLiteral();
    ASTNodePtr parseArrayComp();
    ASTNodePtr parseExprList();

    // assignment / lvalue
    ASTNodePtr parseLValue();
    ASTNodePtr parseAssignOpStmt();

    // print
    ASTNodePtr parsePrintExpr();
    ASTNodePtr parsePrintStmt();

    // match
    ASTNodePtr parseMatchStmt();
    ASTNodePtr parseMatchCaseListOpt();
    ASTNodePtr parseMatchCase();
    ASTNodePtr parseMatchPattern();
    ASTNodePtr parseMatchBody();

    // control
    ASTNodePtr parseIfStmt();
    ASTNodePtr parseIfElsePart();
    ASTNodePtr parseWhileStmt();
    ASTNodePtr parseForStmt();
    ASTNodePtr parseForeachStmt();
    ASTNodePtr parseBlock();
    ASTNodePtr parseBlockBody();
    ASTNodePtr parseStmtList();

    // for parts
    ASTNodePtr parseForInit();
    ASTNodePtr parseForCond();
    ASTNodePtr parseForUpdate();

    // expressions
    ASTNodePtr parseExpr();
    ASTNodePtr parseCondExpr();
    ASTNodePtr parseBoolOr();
    ASTNodePtr parseBoolAnd();
    ASTNodePtr parseRelExpr();
    ASTNodePtr parseArithExpr();
    ASTNodePtr parseTerm();
    ASTNodePtr parsePower();
    ASTNodePtr parseFactor();

    // builtin
    ASTNodePtr parseBuiltinCall();
    ASTNodePtr parseBuiltinName();
    ASTNodePtr parseArgList();
};
