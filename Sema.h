#pragma once
#include "parser.h"
#include <unordered_map>
#include <string>
#include <iostream>

enum class ValueType {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_ARRAY,
    TYPE_UNKNOWN
};

class Sema {
public:
    Sema(const ASTNodePtr& root);

    bool analyze();
    bool hasErrors() const { return hasError; }

private:
    ASTNodePtr root;
    bool hasError = false;

    // symbol table: name -> type
    std::unordered_map<std::string, ValueType> symbols;

    // visitor entry points
    void visit(const ASTNodePtr& node);
    void visitStmtList(const ASTNodePtr& node);
    void visitVarDecl(const ASTNodePtr& node);
    void visitArrayDecl(const ASTNodePtr& node);

    // updated for Phase1 assignment syntax
    void visitAssignOpStmt(const ASTNodePtr& node);

    void visitIf(const ASTNodePtr& node);
    void visitWhile(const ASTNodePtr& node);
    void visitFor(const ASTNodePtr& node);
    void visitForeach(const ASTNodePtr& node);
    void visitPrint(const ASTNodePtr& node);
    void visitBlock(const ASTNodePtr& node);

    // expression checks
    ValueType checkExpr(const ASTNodePtr& node);
    ValueType checkBoolExpr(const ASTNodePtr& node);
    ValueType checkArith(const ASTNodePtr& node);
    ValueType checkRel(const ASTNodePtr& node);

    // helpers
    void report(const Token& tok, const std::string& msg);
    bool isVarDefined(const std::string& name, const Token& tok);
    ValueType stringToType(const std::string& s);
};
