#pragma once
#include "parser.h"
#include <iostream>

inline const char* astNodeTypeToString(ASTNodeType t) {
    switch (t) {
        case ASTNodeType::Program:      return "Program";
        case ASTNodeType::StmtList:     return "StmtList";
        case ASTNodeType::VarDecl:      return "VarDecl";
        case ASTNodeType::VarDeclItem:  return "VarDeclItem";
        case ASTNodeType::ArrayDecl:    return "ArrayDecl";
        case ASTNodeType::ArrayLiteral: return "ArrayLiteral";
        case ASTNodeType::AssignStmt:   return "AssignStmt";
        case ASTNodeType::UnaryStmt:    return "UnaryStmt";
        case ASTNodeType::PrintStmt:    return "PrintStmt";
        case ASTNodeType::Block:        return "Block";
        case ASTNodeType::IfStmt:       return "IfStmt";
        case ASTNodeType::WhileStmt:    return "WhileStmt";
        case ASTNodeType::ForStmt:      return "ForStmt";
        case ASTNodeType::ForeachStmt:  return "ForeachStmt";
        case ASTNodeType::ForInit:      return "ForInit";
        case ASTNodeType::ForCond:      return "ForCond";
        case ASTNodeType::ForUpdate:    return "ForUpdate";
        case ASTNodeType::UnaryOp:      return "UnaryOp";
        case ASTNodeType::BoolExpr:     return "BoolExpr";
        case ASTNodeType::BoolOr:       return "BoolOr";
        case ASTNodeType::BoolAnd:      return "BoolAnd";
        case ASTNodeType::RelExpr:      return "RelExpr";
        case ASTNodeType::BitOr:        return "BitOr";
        case ASTNodeType::BitAnd:       return "BitAnd";
        case ASTNodeType::ArithExpr:    return "ArithExpr";
        case ASTNodeType::Term:         return "Term";
        case ASTNodeType::Power:        return "Power";
        case ASTNodeType::BoolLiteral:  return "BoolLiteral";
        case ASTNodeType::IntLiteral:   return "IntLiteral";
        case ASTNodeType::FloatLiteral: return "FloatLiteral";
        case ASTNodeType::Identifier:   return "Identifier";
        case ASTNodeType::BuiltinCall:  return "BuiltinCall";
        case ASTNodeType::ArgList:      return "ArgList";
        default:                        return "Unknown";
    }
}

inline void printAST(const ASTNodePtr& node, int indent = 0) {
    if (!node) return;
    std::string pad(indent, ' ');
    std::cout << pad
              << astNodeTypeToString(node->type)   // نام نود
              << " : "
              << tokenTypeToString(node->token.type)  // نوع توکن
              << " [" << node->token.lexeme << "]\n";

    for (auto& child : node->children)
        printAST(child, indent + 2);
}
