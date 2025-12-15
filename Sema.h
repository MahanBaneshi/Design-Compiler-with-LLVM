// Sema.h
#pragma once
#include "parser.h"
#include <unordered_map>
#include <string>
#include <vector>

enum class ValueType {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_ARRAY,
    TYPE_UNKNOWN
};

struct SemaError {
    int line;
    int column;
    std::string message;
};

class Sema {
public:
    explicit Sema(const ASTNodePtr& root);

    bool analyze();
    bool hasErrors() const { return hasError; }
    const std::vector<SemaError>& getErrors() const { return errors; }

private:
    ASTNodePtr root;

    bool hasError = false;
    std::vector<SemaError> errors;

    std::unordered_map<std::string, ValueType> symbols;

    void visit(const ASTNodePtr& node);
    void visitStmtList(const ASTNodePtr& node);
    void visitVarDecl(const ASTNodePtr& node);
    void visitArrayDecl(const ASTNodePtr& node);
    void visitAssignOpStmt(const ASTNodePtr& node);
    void visitIf(const ASTNodePtr& node);
    void visitWhile(const ASTNodePtr& node);
    void visitFor(const ASTNodePtr& node);
    void visitForeach(const ASTNodePtr& node);
    void visitPrint(const ASTNodePtr& node);
    void visitBlock(const ASTNodePtr& node);

    ValueType checkExpr(const ASTNodePtr& node);

    void report(const Token& tok, const std::string& msg);

    bool isVarDefined(const std::string& name, const Token& tok);
    ValueType stringToType(const std::string& s);
};
