// CodeGen.h
#pragma once

#include <string>
#include <memory>
#include <unordered_map>

#include "parser.h"
#include "Sema.h"

// =======================
// LLVM HEADERS
// =======================
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/BasicBlock.h>

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>

class CodeGen {
public:
    explicit CodeGen(const std::string& moduleName = "MainModule");

    bool compile(const ASTNodePtr& root);
    int runMain();
    bool saveIRToFile(const std::string& path) const;
    bool emitObjectFile(const std::string& path) const;

private:
    // --- Statements ---
    void genStmt(const ASTNodePtr& node);
    void genStmtList(const ASTNodePtr& node);
    void genVarDecl(const ASTNodePtr& node);

    // updated for Phase1 assignment syntax
    void genAssignOpStmt(const ASTNodePtr& node);

    void genPrint(const ASTNodePtr& node);
    void genBlock(const ASTNodePtr& node);
    void genIf(const ASTNodePtr& node);
    void genWhile(const ASTNodePtr& node);
    void genFor(const ASTNodePtr& node);
    void genForeach(const ASTNodePtr& node);

    // --- Expressions ---
    llvm::Value* genExpr(const ASTNodePtr& node);
    llvm::Value* genBoolExpr(const ASTNodePtr& node);
    llvm::Value* genBoolOr(const ASTNodePtr& node);
    llvm::Value* genBoolAnd(const ASTNodePtr& node);
    llvm::Value* genRel(const ASTNodePtr& node);
    llvm::Value* genArith(const ASTNodePtr& node);
    llvm::Value* genTerm(const ASTNodePtr& node);
    llvm::Value* genPower(const ASTNodePtr& node);
    llvm::Value* genFactor(const ASTNodePtr& node);

    // --- Helpers ---
    llvm::Type* mapType(const std::string& tname);
    llvm::Value* loadVar(const std::string& name);
    llvm::Value* storeVar(const std::string& name, llvm::Value* val);

    llvm::Value* implicitCast(llvm::Value* v, llvm::Type* targetType);
    bool isFloat(llvm::Type* t);
    bool isInt(llvm::Type* t);
    bool isBool(llvm::Type* t);

public:
    std::unique_ptr<llvm::LLVMContext> Ctx;
    std::unique_ptr<llvm::Module> Mod;
    llvm::IRBuilder<>* Builder;

    llvm::Function* MainFn = nullptr;

    std::unordered_map<std::string, llvm::Value*> namedValues;

    std::string moduleName;
};
