// CodeGen.cpp
#include "CodeGen.h"
#include <iostream>
#include <system_error>

#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/ExecutionEngine/GenericValue.h>

#include "cfg_gen.h"
#include "callgraph_gen.h"

static void initLLVMTargetOnce() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

// =====================================================
// Constructor
// =====================================================
CodeGen::CodeGen(const std::string& moduleName)
    : moduleName(moduleName) {
    initLLVMTargetOnce();

    Ctx = std::make_unique<llvm::LLVMContext>();
    Mod = std::make_unique<llvm::Module>(moduleName, *Ctx);
    Builder = std::make_unique<llvm::IRBuilder<>>(*Ctx);
}

// =====================================================
// Helpers
// =====================================================
llvm::Type* CodeGen::mapTypeName(const std::string& tname) {
    if (tname == "int") return llvm::Type::getInt32Ty(*Ctx);
    if (tname == "float") return llvm::Type::getDoubleTy(*Ctx);
    if (tname == "bool") return llvm::Type::getInt1Ty(*Ctx);
    return nullptr;
}

bool CodeGen::isInt(llvm::Type* t) const { return t && t->isIntegerTy(32); }
bool CodeGen::isBool(llvm::Type* t) const { return t && t->isIntegerTy(1); }
bool CodeGen::isFloat(llvm::Type* t) const { return t && t->isDoubleTy(); }

llvm::Value* CodeGen::implicitCast(llvm::Value* v, llvm::Type* targetType) {
    if (!v || !targetType) return nullptr;
    llvm::Type* src = v->getType();
    if (src == targetType) return v;

    if (isInt(src) && isFloat(targetType)) return Builder->CreateSIToFP(v, targetType);
    if (isFloat(src) && isInt(targetType)) return Builder->CreateFPToSI(v, targetType);

    if (isBool(src) && isInt(targetType)) return Builder->CreateZExt(v, targetType);
    if (isInt(src) && isBool(targetType)) return Builder->CreateICmpNE(v, llvm::ConstantInt::get(src, 0));

    if (isFloat(src) && isBool(targetType)) return Builder->CreateFCmpONE(v, llvm::ConstantFP::get(src, 0.0));
    if (isBool(src) && isFloat(targetType)) return Builder->CreateUIToFP(v, targetType);

    return v;
}

llvm::Function* CodeGen::getOrCreatePrintI32() {
    if (auto* fn = Mod->getFunction("print_i32")) return fn;
    auto* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(*Ctx), { llvm::Type::getInt32Ty(*Ctx) }, false);
    return llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "print_i32", Mod.get());
}

llvm::Function* CodeGen::getOrCreatePrintF64() {
    if (auto* fn = Mod->getFunction("print_f64")) return fn;
    auto* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(*Ctx), { llvm::Type::getDoubleTy(*Ctx) }, false);
    return llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "print_f64", Mod.get());
}

llvm::Function* CodeGen::getOrCreatePrintBool() {
    if (auto* fn = Mod->getFunction("print_bool")) return fn;
    auto* FT = llvm::FunctionType::get(llvm::Type::getVoidTy(*Ctx), { llvm::Type::getInt1Ty(*Ctx) }, false);
    return llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "print_bool", Mod.get());
}

llvm::Value* CodeGen::loadVar(const std::string& name) {
    auto it = namedValues.find(name);
    if (it == namedValues.end()) return nullptr;
    llvm::Value* ptr = it->second;
    llvm::Type* ty = ptr->getType()->getPointerElementType();
    return Builder->CreateLoad(ty, ptr, name + ".val");
}

llvm::Value* CodeGen::storeVar(const std::string& name, llvm::Value* val) {
    auto it = namedValues.find(name);
    if (it == namedValues.end()) return nullptr;
    llvm::Value* ptr = it->second;
    llvm::Type* dstTy = ptr->getType()->getPointerElementType();
    llvm::Value* v = implicitCast(val, dstTy);
    Builder->CreateStore(v, ptr);
    return v;
}

llvm::Value* CodeGen::genLValuePtr(const ASTNodePtr& node) {
    if (!node) return nullptr;

    if (node->type == ASTNodeType::LValue) {
        if (node->children.empty()) return nullptr;
        if (node->children.size() == 1) return genLValuePtr(node->children[0]);
        if (node->children.size() == 2) {
            auto* basePtr = genLValuePtr(node->children[0]);
            if (!basePtr) return nullptr;
            auto* idxVal = genExpr(node->children[1]);
            if (!idxVal) return nullptr;
            idxVal = implicitCast(idxVal, llvm::Type::getInt32Ty(*Ctx));
            llvm::Type* elemTy = basePtr->getType()->getPointerElementType()->getPointerElementType();
            return Builder->CreateGEP(elemTy, basePtr, { llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 0), idxVal });
        }
        return nullptr;
    }

    if (node->type == ASTNodeType::Identifier) {
        auto it = namedValues.find(node->token.lexeme);
        if (it == namedValues.end()) return nullptr;
        return it->second;
    }

    return nullptr;
}

// =====================================================
// compile
// =====================================================
bool CodeGen::compile(const ASTNodePtr& root) {
    llvm::Type* i32 = llvm::Type::getInt32Ty(*Ctx);

    MainFn = llvm::Function::Create(
        llvm::FunctionType::get(i32, false),
        llvm::Function::ExternalLinkage,
        "main",
        Mod.get()
    );

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*Ctx, "entry", MainFn);
    Builder->SetInsertPoint(entry);

    if (root) genStmt(root);

    Builder->CreateRet(llvm::ConstantInt::get(i32, 0));

    if (llvm::verifyModule(*Mod, &llvm::errs())) {
        std::cerr << "LLVM IR verification failed.\n";
        return false;
    }

    if (MainFn) writeCFG(MainFn, "cfg_main.dot");
    writeCallGraph(Mod.get(), "callgraph.dot");
    return true;
}

// =====================================================
// Statements
// =====================================================
void CodeGen::genStmt(const ASTNodePtr& node) {
    if (!node) return;

    switch (node->type) {
        case ASTNodeType::Program:
            for (auto& ch : node->children) genStmt(ch);
            break;

        case ASTNodeType::StmtList:
            genStmtList(node);
            break;

        case ASTNodeType::Block:
            genBlock(node);
            break;

        case ASTNodeType::VarDecl:
            genVarDecl(node);
            break;

        case ASTNodeType::TypedVarDecl:
            genTypedVarDecl(node);
            break;

        case ASTNodeType::ArrayDecl:
            genArrayDecl(node);
            break;

        case ASTNodeType::AssignOpStmt:
            genAssignOpStmt(node);
            break;

        case ASTNodeType::PrintStmt:
            genPrint(node);
            break;

        case ASTNodeType::MatchStmt:
            genMatch(node);
            break;

        case ASTNodeType::IfStmt:
            genIf(node);
            break;

        case ASTNodeType::WhileStmt:
            genWhile(node);
            break;

        case ASTNodeType::ForStmt:
            genFor(node);
            break;

        case ASTNodeType::ForeachStmt:
            genForeach(node);
            break;

        case ASTNodeType::ForInit:
        case ASTNodeType::ForCond:
        case ASTNodeType::ForUpdate:
            for (auto& ch : node->children) genStmt(ch);
            break;

        default:
            break;
    }
}

void CodeGen::genStmtList(const ASTNodePtr& node) {
    for (auto& st : node->children) genStmt(st);
}

void CodeGen::genBlock(const ASTNodePtr& node) {
    if (!node->children.empty()) genStmt(node->children[0]);
}

void CodeGen::genVarDecl(const ASTNodePtr& node) {
    if (node->children.empty()) return;
    auto list = node->children[0];
    for (auto& item : list->children) {
        std::string name = item->children[0]->token.lexeme;
        std::string tname = item->children[1]->token.lexeme;
        llvm::Type* ty = mapTypeName(tname);
        if (!ty) continue;

        llvm::Value* alloc = Builder->CreateAlloca(ty, nullptr, name);
        namedValues[name] = alloc;

        if (item->children.size() >= 3) {
            llvm::Value* initVal = genExpr(item->children[2]);
            if (initVal) storeVar(name, initVal);
        }
    }
}

void CodeGen::genTypedVarDecl(const ASTNodePtr& node) {
    if (node->children.size() < 2) return;
    std::string tname = node->children[0]->token.lexeme;
    llvm::Type* ty = mapTypeName(tname);
    if (!ty) return;

    auto list = node->children[1];
    for (auto& item : list->children) {
        std::string name = item->children[0]->token.lexeme;
        llvm::Value* alloc = Builder->CreateAlloca(ty, nullptr, name);
        namedValues[name] = alloc;

        if (item->children.size() >= 2) {
            llvm::Value* initVal = genExpr(item->children[1]);
            if (initVal) storeVar(name, initVal);
        }
    }
}

void CodeGen::genArrayDecl(const ASTNodePtr& node) {
    if (node->children.empty()) return;
    std::string name = node->children[0]->token.lexeme;

    llvm::Type* arrTy = llvm::Type::getInt8PtrTy(*Ctx);
    llvm::Value* alloc = Builder->CreateAlloca(arrTy, nullptr, name);
    namedValues[name] = alloc;

    if (node->children.size() >= 2) {
        llvm::Value* v = genArrayInit(node->children[1]);
        if (!v) v = llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*Ctx));
        Builder->CreateStore(v, alloc);
    } else {
        Builder->CreateStore(llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*Ctx)), alloc);
    }
}

llvm::Value* CodeGen::genArrayInit(const ASTNodePtr& node) {
    if (!node) return llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*Ctx));
    if (node->type == ASTNodeType::ArrayLiteral) return genArrayLiteral(node);
    if (node->type == ASTNodeType::ArrayComp) return genArrayComp(node);
    return llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*Ctx));
}

llvm::Value* CodeGen::genArrayLiteral(const ASTNodePtr&) {
    return llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*Ctx));
}

llvm::Value* CodeGen::genArrayComp(const ASTNodePtr&) {
    return llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*Ctx));
}

// =====================================================
// AssignOp (supports LValue destination)
// =====================================================
void CodeGen::genAssignOpStmt(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    TokenType op = node->token.type;

    llvm::Value* dstPtr = genLValuePtr(node->children[0]);
    if (!dstPtr) return;

    auto loadDst = [&]() -> llvm::Value* {
        llvm::Type* ty = dstPtr->getType()->getPointerElementType();
        return Builder->CreateLoad(ty, dstPtr);
    };
    auto storeDst = [&](llvm::Value* v) {
        llvm::Type* ty = dstPtr->getType()->getPointerElementType();
        llvm::Value* vv = implicitCast(v, ty);
        Builder->CreateStore(vv, dstPtr);
    };

    if (op == TokenType::KW_INC || op == TokenType::KW_DEC) {
        llvm::Value* cur = loadDst();
        if (!cur) return;

        if (isFloat(cur->getType())) {
            llvm::Value* one = llvm::ConstantFP::get(cur->getType(), 1.0);
            llvm::Value* next = (op == TokenType::KW_INC) ? Builder->CreateFAdd(cur, one) : Builder->CreateFSub(cur, one);
            storeDst(next);
        } else {
            llvm::Value* curi = implicitCast(cur, llvm::Type::getInt32Ty(*Ctx));
            llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 1);
            llvm::Value* next = (op == TokenType::KW_INC) ? Builder->CreateAdd(curi, one) : Builder->CreateSub(curi, one);
            storeDst(next);
        }
        return;
    }

    if (node->children.size() < 2) return;
    llvm::Value* y = genExpr(node->children[1]);
    if (!y) return;

    if (op == TokenType::KW_PLE || op == TokenType::KW_MIE) {
        llvm::Value* cur = loadDst();
        if (!cur) return;

        bool f = isFloat(cur->getType()) || isFloat(y->getType());
        if (f) {
            cur = implicitCast(cur, llvm::Type::getDoubleTy(*Ctx));
            y = implicitCast(y, llvm::Type::getDoubleTy(*Ctx));
            llvm::Value* next = (op == TokenType::KW_PLE) ? Builder->CreateFAdd(cur, y) : Builder->CreateFSub(cur, y);
            storeDst(next);
        } else {
            cur = implicitCast(cur, llvm::Type::getInt32Ty(*Ctx));
            y = implicitCast(y, llvm::Type::getInt32Ty(*Ctx));
            llvm::Value* next = (op == TokenType::KW_PLE) ? Builder->CreateAdd(cur, y) : Builder->CreateSub(cur, y);
            storeDst(next);
        }
        return;
    }

    if (node->children.size() < 3) return;
    llvm::Value* z = genExpr(node->children[2]);
    if (!z) return;

    if (op == TokenType::KW_AND || op == TokenType::KW_OR) {
        y = implicitCast(y, llvm::Type::getInt1Ty(*Ctx));
        z = implicitCast(z, llvm::Type::getInt1Ty(*Ctx));
        llvm::Value* r = (op == TokenType::KW_AND) ? Builder->CreateAnd(y, z) : Builder->CreateOr(y, z);
        storeDst(r);
        return;
    }

    bool f = isFloat(y->getType()) || isFloat(z->getType());
    if (f) {
        y = implicitCast(y, llvm::Type::getDoubleTy(*Ctx));
        z = implicitCast(z, llvm::Type::getDoubleTy(*Ctx));
        llvm::Value* r = nullptr;
        if (op == TokenType::KW_ADD) r = Builder->CreateFAdd(y, z);
        else if (op == TokenType::KW_SUB) r = Builder->CreateFSub(y, z);
        else if (op == TokenType::KW_MUL) r = Builder->CreateFMul(y, z);
        else if (op == TokenType::KW_DIV) r = Builder->CreateFDiv(y, z);
        else return;
        storeDst(r);
    } else {
        y = implicitCast(y, llvm::Type::getInt32Ty(*Ctx));
        z = implicitCast(z, llvm::Type::getInt32Ty(*Ctx));
        llvm::Value* r = nullptr;
        if (op == TokenType::KW_ADD) r = Builder->CreateAdd(y, z);
        else if (op == TokenType::KW_SUB) r = Builder->CreateSub(y, z);
        else if (op == TokenType::KW_MUL) r = Builder->CreateMul(y, z);
        else if (op == TokenType::KW_DIV) r = Builder->CreateSDiv(y, z);
        else if (op == TokenType::KW_MOD) r = Builder->CreateSRem(y, z);
        else return;
        storeDst(r);
    }
}

// =====================================================
// Print
// =====================================================
void CodeGen::genPrint(const ASTNodePtr& node) {
    if (!node || node->children.empty()) return;
    llvm::Value* v = genExpr(node->children[0]);
    if (!v) return;

    if (isInt(v->getType())) {
        v = implicitCast(v, llvm::Type::getInt32Ty(*Ctx));
        Builder->CreateCall(getOrCreatePrintI32(), { v });
        return;
    }
    if (isFloat(v->getType())) {
        v = implicitCast(v, llvm::Type::getDoubleTy(*Ctx));
        Builder->CreateCall(getOrCreatePrintF64(), { v });
        return;
    }
    if (isBool(v->getType())) {
        v = implicitCast(v, llvm::Type::getInt1Ty(*Ctx));
        Builder->CreateCall(getOrCreatePrintBool(), { v });
        return;
    }
}

// =====================================================
// Match (simple lowering to if-else chain)
// =====================================================
void CodeGen::genMatch(const ASTNodePtr& node) {
    if (!node || node->children.size() < 2) return;

    llvm::Value* scrut = genExpr(node->children[0]);
    if (!scrut) return;

    auto* fn = Builder->GetInsertBlock()->getParent();
    auto* endBB = llvm::BasicBlock::Create(*Ctx, "match.end", fn);

    ASTNodePtr caseList = node->children[1];
    llvm::BasicBlock* nextCheck = nullptr;

    for (std::size_t i = 0; i < caseList->children.size(); ++i) {
        ASTNodePtr c = caseList->children[i];
        if (!c || c->children.size() < 2) continue;

        ASTNodePtr pat = c->children[0];
        ASTNodePtr body = c->children[1];

        llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Ctx, "match.body", fn);
        llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*Ctx, "match.next", fn);

        if (nextCheck) Builder->SetInsertPoint(nextCheck);

        llvm::Value* cond = nullptr;

        if (pat->type == ASTNodeType::Underscore) {
            cond = llvm::ConstantInt::getTrue(*Ctx);
        } else if (pat->type == ASTNodeType::IntLiteral) {
            llvm::Value* pv = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), std::stoi(pat->token.lexeme));
            scrut = implicitCast(scrut, llvm::Type::getInt32Ty(*Ctx));
            cond = Builder->CreateICmpEQ(scrut, pv);
        } else if (pat->type == ASTNodeType::BoolLiteral) {
            llvm::Value* pv = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Ctx), pat->token.lexeme == "true");
            scrut = implicitCast(scrut, llvm::Type::getInt1Ty(*Ctx));
            cond = Builder->CreateICmpEQ(scrut, pv);
        } else if (pat->type == ASTNodeType::FloatLiteral) {
            llvm::Value* pv = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*Ctx), std::stod(pat->token.lexeme));
            scrut = implicitCast(scrut, llvm::Type::getDoubleTy(*Ctx));
            cond = Builder->CreateFCmpOEQ(scrut, pv);
        } else {
            cond = llvm::ConstantInt::getFalse(*Ctx);
        }

        Builder->CreateCondBr(cond, bodyBB, elseBB);

        Builder->SetInsertPoint(bodyBB);
        genStmt(body);
        if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(endBB);

        nextCheck = elseBB;

        if (pat->type == ASTNodeType::Underscore) {
            Builder->SetInsertPoint(elseBB);
            Builder->CreateBr(endBB);
            nextCheck = nullptr;
            break;
        }
    }

    if (nextCheck) {
        Builder->SetInsertPoint(nextCheck);
        Builder->CreateBr(endBB);
    }

    Builder->SetInsertPoint(endBB);
}

// =====================================================
// Control flow
// =====================================================
void CodeGen::genIf(const ASTNodePtr& node) {
    if (!node || node->children.size() < 2) return;

    llvm::Function* fn = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*Ctx, "if.then", fn);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*Ctx, "if.else", fn);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(*Ctx, "if.end", fn);

    llvm::Value* cond = genExpr(node->children[0]);
    cond = implicitCast(cond, llvm::Type::getInt1Ty(*Ctx));
    Builder->CreateCondBr(cond, thenBB, elseBB);

    Builder->SetInsertPoint(thenBB);
    genStmt(node->children[1]);
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(endBB);

    Builder->SetInsertPoint(elseBB);
    if (node->children.size() >= 3) genStmt(node->children[2]);
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(endBB);

    Builder->SetInsertPoint(endBB);
}

void CodeGen::genWhile(const ASTNodePtr& node) {
    if (!node || node->children.size() < 2) return;

    llvm::Function* fn = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*Ctx, "while.cond", fn);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Ctx, "while.body", fn);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(*Ctx, "while.end", fn);

    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(node->children[0]);
    cond = implicitCast(cond, llvm::Type::getInt1Ty(*Ctx));
    Builder->CreateCondBr(cond, bodyBB, endBB);

    Builder->SetInsertPoint(bodyBB);
    genStmt(node->children[1]);
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(condBB);

    Builder->SetInsertPoint(endBB);
}

void CodeGen::genFor(const ASTNodePtr& node) {
    if (!node || node->children.size() < 4) return;

    genStmt(node->children[0]);

    llvm::Function* fn = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*Ctx, "for.cond", fn);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Ctx, "for.body", fn);
    llvm::BasicBlock* updBB  = llvm::BasicBlock::Create(*Ctx, "for.update", fn);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(*Ctx, "for.end", fn);

    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(condBB);
    llvm::Value* cond = nullptr;
    if (!node->children[1]->children.empty()) cond = genExpr(node->children[1]->children[0]);
    if (!cond) cond = llvm::ConstantInt::getTrue(*Ctx);
    cond = implicitCast(cond, llvm::Type::getInt1Ty(*Ctx));
    Builder->CreateCondBr(cond, bodyBB, endBB);

    Builder->SetInsertPoint(bodyBB);
    genStmt(node->children[3]);
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(updBB);

    Builder->SetInsertPoint(updBB);
    genStmt(node->children[2]);
    if (!Builder->GetInsertBlock()->getTerminator()) Builder->CreateBr(condBB);

    Builder->SetInsertPoint(endBB);
}

void CodeGen::genForeach(const ASTNodePtr& node) {
    if (!node || node->children.size() < 3) return;
    genStmt(node->children[2]);
}

// =====================================================
// Expressions
// =====================================================
llvm::Value* CodeGen::genExpr(const ASTNodePtr& node) {
    if (!node) return nullptr;

    switch (node->type) {
        case ASTNodeType::Expr:
        case ASTNodeType::CondExpr:
            if (!node->children.empty()) return genExpr(node->children[0]);
            return nullptr;

        case ASTNodeType::BoolOr: return genBoolOr(node);
        case ASTNodeType::BoolAnd: return genBoolAnd(node);
        case ASTNodeType::RelExpr: return genRel(node);
        case ASTNodeType::ArithExpr: return genArith(node);
        case ASTNodeType::Term: return genTerm(node);
        case ASTNodeType::Power: return genPower(node);
        case ASTNodeType::Factor: return genFactor(node);

        case ASTNodeType::IntLiteral:
        case ASTNodeType::FloatLiteral:
        case ASTNodeType::BoolLiteral:
        case ASTNodeType::Identifier:
        case ASTNodeType::BuiltinCall:
        case ASTNodeType::ArgList:
            return genFactor(node);

        default:
            return nullptr;
    }
}

llvm::Value* CodeGen::genCondExpr(const ASTNodePtr& node) { return genExpr(node); }

llvm::Value* CodeGen::genBoolOr(const ASTNodePtr& node) {
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    L = implicitCast(L, llvm::Type::getInt1Ty(*Ctx));
    R = implicitCast(R, llvm::Type::getInt1Ty(*Ctx));
    return Builder->CreateOr(L, R);
}

llvm::Value* CodeGen::genBoolAnd(const ASTNodePtr& node) {
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    L = implicitCast(L, llvm::Type::getInt1Ty(*Ctx));
    R = implicitCast(R, llvm::Type::getInt1Ty(*Ctx));
    return Builder->CreateAnd(L, R);
}

llvm::Value* CodeGen::genRel(const ASTNodePtr& node) {
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    if (!L || !R) return llvm::ConstantInt::getFalse(*Ctx);

    std::string op = node->token.lexeme;

    if (isFloat(L->getType()) || isFloat(R->getType())) {
        L = implicitCast(L, llvm::Type::getDoubleTy(*Ctx));
        R = implicitCast(R, llvm::Type::getDoubleTy(*Ctx));

        if (op == "<") return Builder->CreateFCmpOLT(L, R);
        if (op == ">") return Builder->CreateFCmpOGT(L, R);
        if (op == "<=") return Builder->CreateFCmpOLE(L, R);
        if (op == ">=") return Builder->CreateFCmpOGE(L, R);
        if (op == "==") return Builder->CreateFCmpOEQ(L, R);
        if (op == "!=") return Builder->CreateFCmpONE(L, R);
    } else {
        L = implicitCast(L, llvm::Type::getInt32Ty(*Ctx));
        R = implicitCast(R, llvm::Type::getInt32Ty(*Ctx));

        if (op == "<") return Builder->CreateICmpSLT(L, R);
        if (op == ">") return Builder->CreateICmpSGT(L, R);
        if (op == "<=") return Builder->CreateICmpSLE(L, R);
        if (op == ">=") return Builder->CreateICmpSGE(L, R);
        if (op == "==") return Builder->CreateICmpEQ(L, R);
        if (op == "!=") return Builder->CreateICmpNE(L, R);
    }

    return llvm::ConstantInt::getFalse(*Ctx);
}

llvm::Value* CodeGen::genArith(const ASTNodePtr& node) {
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    if (!L || !R) return nullptr;

    std::string op = node->token.lexeme;
    bool f = isFloat(L->getType()) || isFloat(R->getType());

    if (f) {
        L = implicitCast(L, llvm::Type::getDoubleTy(*Ctx));
        R = implicitCast(R, llvm::Type::getDoubleTy(*Ctx));
        return (op == "+") ? Builder->CreateFAdd(L, R) : Builder->CreateFSub(L, R);
    }

    L = implicitCast(L, llvm::Type::getInt32Ty(*Ctx));
    R = implicitCast(R, llvm::Type::getInt32Ty(*Ctx));
    return (op == "+") ? Builder->CreateAdd(L, R) : Builder->CreateSub(L, R);
}

llvm::Value* CodeGen::genTerm(const ASTNodePtr& node) {
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    if (!L || !R) return nullptr;

    std::string op = node->token.lexeme;
    bool f = isFloat(L->getType()) || isFloat(R->getType());

    if (f) {
        L = implicitCast(L, llvm::Type::getDoubleTy(*Ctx));
        R = implicitCast(R, llvm::Type::getDoubleTy(*Ctx));
        if (op == "*") return Builder->CreateFMul(L, R);
        if (op == "/") return Builder->CreateFDiv(L, R);
        return nullptr;
    }

    L = implicitCast(L, llvm::Type::getInt32Ty(*Ctx));
    R = implicitCast(R, llvm::Type::getInt32Ty(*Ctx));
    if (op == "*") return Builder->CreateMul(L, R);
    if (op == "/") return Builder->CreateSDiv(L, R);
    if (op == "%") return Builder->CreateSRem(L, R);
    return nullptr;
}

llvm::Value* CodeGen::genPower(const ASTNodePtr& node) {
    llvm::Value* base = genExpr(node->children[0]);
    llvm::Value* exp = genExpr(node->children[1]);
    if (!base || !exp) return nullptr;

    base = implicitCast(base, llvm::Type::getInt32Ty(*Ctx));
    exp  = implicitCast(exp,  llvm::Type::getInt32Ty(*Ctx));

    llvm::Value* resultAlloca = Builder->CreateAlloca(llvm::Type::getInt32Ty(*Ctx));
    llvm::Value* counterAlloca = Builder->CreateAlloca(llvm::Type::getInt32Ty(*Ctx));

    Builder->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 1), resultAlloca);
    Builder->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 0), counterAlloca);

    llvm::Function* fn = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* condBB  = llvm::BasicBlock::Create(*Ctx, "pow.cond", fn);
    llvm::BasicBlock* bodyBB  = llvm::BasicBlock::Create(*Ctx, "pow.body", fn);
    llvm::BasicBlock* endBB   = llvm::BasicBlock::Create(*Ctx, "pow.end", fn);

    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(condBB);
    llvm::Value* c = Builder->CreateLoad(llvm::Type::getInt32Ty(*Ctx), counterAlloca);
    llvm::Value* cond = Builder->CreateICmpSLT(c, exp);
    Builder->CreateCondBr(cond, bodyBB, endBB);

    Builder->SetInsertPoint(bodyBB);
    llvm::Value* r = Builder->CreateLoad(llvm::Type::getInt32Ty(*Ctx), resultAlloca);
    Builder->CreateStore(Builder->CreateMul(r, base), resultAlloca);
    Builder->CreateStore(Builder->CreateAdd(c, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 1)), counterAlloca);
    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(endBB);
    return Builder->CreateLoad(llvm::Type::getInt32Ty(*Ctx), resultAlloca);
}

llvm::Value* CodeGen::genBuiltinCall(const ASTNodePtr&) {
    return nullptr;
}

llvm::Value* CodeGen::genFactor(const ASTNodePtr& node) {
    if (!node) return nullptr;

    if (node->type == ASTNodeType::Factor) {
        if (node->children.empty()) return nullptr;
        if (node->children.size() == 1) return genExpr(node->children[0]);
        if (node->children.size() == 2) {
            llvm::Value* v = genExpr(node->children[0]);
            if (!v) return nullptr;
            std::string op = node->token.lexeme;
            if (op == "+") return v;

            if (isFloat(v->getType())) return Builder->CreateFNeg(implicitCast(v, llvm::Type::getDoubleTy(*Ctx)));
            v = implicitCast(v, llvm::Type::getInt32Ty(*Ctx));
            return Builder->CreateNeg(v);
        }
    }

    switch (node->type) {
        case ASTNodeType::IntLiteral:
            return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), std::stoi(node->token.lexeme));
        case ASTNodeType::FloatLiteral:
            return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*Ctx), std::stod(node->token.lexeme));
        case ASTNodeType::BoolLiteral:
            return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Ctx), node->token.lexeme == "true");
        case ASTNodeType::Identifier:
            return loadVar(node->token.lexeme);
        case ASTNodeType::BuiltinCall:
            return genBuiltinCall(node);
        default:
            return nullptr;
    }
}

// =====================================================
// saveIRToFile / emitObjectFile / runMain
// =====================================================
bool CodeGen::saveIRToFile(const std::string& path) const {
    if (!Mod) return false;

    std::error_code EC;
    llvm::raw_fd_ostream out(path, EC, llvm::sys::fs::OF_Text);
    if (EC) return false;

    Mod->print(out, nullptr);
    return true;
}

bool CodeGen::emitObjectFile(const std::string& path) const {
    if (!Mod) return false;

    std::string triple = llvm::sys::getDefaultTargetTriple();
    Mod->setTargetTriple(triple);

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) return false;

    std::string cpu = "generic";
    std::string features;
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();

    std::unique_ptr<llvm::TargetMachine> TM(
        target->createTargetMachine(triple, cpu, features, opt, RM));

    Mod->setDataLayout(TM->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(path, EC, llvm::sys::fs::OF_None);
    if (EC) return false;

    llvm::legacy::PassManager pass;
    if (TM->addPassesToEmitFile(pass, dest, nullptr, llvm::CGFT_ObjectFile)) return false;

    pass.run(*Mod);
    dest.flush();
    return true;
}

int CodeGen::runMain() {
    if (!MainFn || !Mod) return 1;

    std::string errStr;
    std::unique_ptr<llvm::Module> M = std::move(Mod);

    llvm::EngineBuilder builder(std::move(M));
    builder.setErrorStr(&errStr);
    builder.setEngineKind(llvm::EngineKind::JIT);

    llvm::ExecutionEngine* EE = builder.create();
    if (!EE) return 1;

    std::vector<llvm::GenericValue> noArgs;
    llvm::GenericValue ret = EE->runFunction(MainFn, noArgs);
    return static_cast<int>(ret.IntVal.getSExtValue());
}
