#pragma once

#include "parser.h"
#include <iostream>
#include <string>

inline const char* astNodeTypeToString(ASTNodeType t) {
    switch (t) {
        case ASTNodeType::Program: return "Program";
        case ASTNodeType::StmtList: return "StmtList";
        case ASTNodeType::Block: return "Block";

        case ASTNodeType::VarDecl: return "VarDecl";
        case ASTNodeType::VarDeclList: return "VarDeclList";
        case ASTNodeType::VarDeclItem: return "VarDeclItem";

        case ASTNodeType::TypedVarDecl: return "TypedVarDecl";
        case ASTNodeType::TypedVarDeclItem: return "TypedVarDeclItem";

        case ASTNodeType::ArrayDecl: return "ArrayDecl";
        case ASTNodeType::ArrayInit: return "ArrayInit";
        case ASTNodeType::ArrayLiteral: return "ArrayLiteral";
        case ASTNodeType::ArrayComp: return "ArrayComp";
        case ASTNodeType::ExprList: return "ExprList";

        case ASTNodeType::AssignOpStmt: return "AssignOpStmt";
        case ASTNodeType::PrintStmt: return "PrintStmt";

        case ASTNodeType::MatchStmt: return "MatchStmt";
        case ASTNodeType::MatchCaseList: return "MatchCaseList";
        case ASTNodeType::MatchCase: return "MatchCase";
        case ASTNodeType::MatchPattern: return "MatchPattern";
        case ASTNodeType::MatchBody: return "MatchBody";

        case ASTNodeType::IfStmt: return "IfStmt";
        case ASTNodeType::WhileStmt: return "WhileStmt";
        case ASTNodeType::ForStmt: return "ForStmt";
        case ASTNodeType::ForeachStmt: return "ForeachStmt";

        case ASTNodeType::ForInit: return "ForInit";
        case ASTNodeType::ForCond: return "ForCond";
        case ASTNodeType::ForUpdate: return "ForUpdate";

        case ASTNodeType::Expr: return "Expr";
        case ASTNodeType::CondExpr: return "CondExpr";
        case ASTNodeType::BoolOr: return "BoolOr";
        case ASTNodeType::BoolAnd: return "BoolAnd";
        case ASTNodeType::RelExpr: return "RelExpr";
        case ASTNodeType::ArithExpr: return "ArithExpr";
        case ASTNodeType::Term: return "Term";
        case ASTNodeType::Power: return "Power";
        case ASTNodeType::Factor: return "Factor";

        case ASTNodeType::BuiltinCall: return "BuiltinCall";
        case ASTNodeType::ArgList: return "ArgList";

        case ASTNodeType::LValue: return "LValue";

        case ASTNodeType::Identifier: return "Identifier";
        case ASTNodeType::IntLiteral: return "IntLiteral";
        case ASTNodeType::FloatLiteral: return "FloatLiteral";
        case ASTNodeType::BoolLiteral: return "BoolLiteral";
        case ASTNodeType::StringLiteral: return "StringLiteral";
        case ASTNodeType::Underscore: return "Underscore";

        default: return "Unknown";
    }
}

inline void printAST(const ASTNodePtr& node, int indent = 0) {
    if (!node) return;
    std::string pad(static_cast<std::size_t>(indent), ' ');
    std::cout << pad << astNodeTypeToString(node->type)
              << " : " << tokenTypeToString(node->token.type)
              << " [" << node->token.lexeme << "]\n";
    for (const auto& child : node->children) printAST(child, indent + 2);
}
