// CodeGen.cpp
#include "CodeGen.h"
#include <iostream>
#include <system_error>

#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Host.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/TargetRegistry.h>
#include "cfg_gen.h"
#include "callgraph_gen.h"

// فقط یک‌بار init کردن target و JIT
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
    : moduleName(moduleName)
{
    initLLVMTargetOnce();

    Ctx = std::make_unique<llvm::LLVMContext>();
    Mod = std::make_unique<llvm::Module>(moduleName, *Ctx);

    Builder = new llvm::IRBuilder<>(*Ctx);

    std::cout << "CodeGen initialized.\n";
}

// =====================================================
// Type mapping
// =====================================================
llvm::Type* CodeGen::mapType(const std::string& tname)
{
    if (tname == "int")   return llvm::Type::getInt32Ty(*Ctx);
    if (tname == "float") return llvm::Type::getDoubleTy(*Ctx);
    if (tname == "bool")  return llvm::Type::getInt1Ty(*Ctx);

    std::cerr << "Unknown type: " << tname << "\n";
    return nullptr;
}

bool CodeGen::isInt(llvm::Type* t)  { return t->isIntegerTy() && t->getIntegerBitWidth() == 32; }
bool CodeGen::isBool(llvm::Type* t) { return t->isIntegerTy() && t->getIntegerBitWidth() == 1; }
bool CodeGen::isFloat(llvm::Type* t){ return t->isDoubleTy(); }

// =====================================================
// implicitCast
// =====================================================
llvm::Value* CodeGen::implicitCast(llvm::Value* v, llvm::Type* target)
{
    llvm::Type* src = v->getType();
    if (src == target) return v;

    if (isInt(src) && isFloat(target))
        return Builder->CreateSIToFP(v, target);

    if (isFloat(src) && isInt(target))
        return Builder->CreateFPToSI(v, target);

    if (isBool(src) && isInt(target))
        return Builder->CreateZExt(v, target);

    if (isInt(src) && isBool(target))
        return Builder->CreateICmpNE(v, llvm::ConstantInt::get(src, 0));

    if (isFloat(src) && isBool(target))
        return Builder->CreateFCmpONE(v, llvm::ConstantFP::get(src, 0.0));

    std::cerr << "Unsupported cast\n";
    return v;
}

// =====================================================
// loadVar / storeVar
// =====================================================
llvm::Value* CodeGen::loadVar(const std::string& name)
{
    if (!namedValues.count(name)) {
        std::cerr << "ERROR: variable not found: " << name << "\n";
        return nullptr;
    }
    llvm::Value* alloc = namedValues[name];
    llvm::Type* ty = alloc->getType()->getPointerElementType();
    return Builder->CreateLoad(ty, alloc);
}

llvm::Value* CodeGen::storeVar(const std::string& name, llvm::Value* val)
{
    if (!namedValues.count(name)) {
        std::cerr << "ERROR: variable not found: " << name << "\n";
        return nullptr;
    }
    llvm::Value* alloc = namedValues[name];
    llvm::Type* destTy = alloc->getType()->getPointerElementType();
    val = implicitCast(val, destTy);
    Builder->CreateStore(val, alloc);
    return val;
}

// =====================================================
// compile
// =====================================================
bool CodeGen::compile(const ASTNodePtr& root)
{
    llvm::Type* i32 = llvm::Type::getInt32Ty(*Ctx);

    MainFn = llvm::Function::Create(
        llvm::FunctionType::get(i32, false),
        llvm::Function::ExternalLinkage,
        "main",
        Mod.get());

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*Ctx, "entry", MainFn);
    Builder->SetInsertPoint(entry);

    for (auto& child : root->children)
        genStmt(child);

    Builder->CreateRet(llvm::ConstantInt::get(i32, 0));

    if (llvm::verifyModule(*Mod, &llvm::errs())) {
        std::cerr << "LLVM IR verification failed.\n";
        return false;
    }

    if (MainFn)
        writeCFG(MainFn, "cfg_main.dot");
    writeCallGraph(Mod.get(), "callgraph.dot");
    return true;
}

// =====================================================
// Statement dispatcher
// =====================================================
void CodeGen::genStmt(const ASTNodePtr& node)
{
    switch (node->type)
    {
    case ASTNodeType::StmtList:
        genStmtList(node);
        break;

    case ASTNodeType::VarDecl:
        genVarDecl(node);
        break;

    case ASTNodeType::AssignOpStmt:
        genAssignOpStmt(node);
        break;

    case ASTNodeType::PrintStmt:
        genPrint(node);
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

    case ASTNodeType::Block:
        genBlock(node);
        break;

    case ASTNodeType::ForInit:
    case ASTNodeType::ForUpdate:
    case ASTNodeType::ForCond:
        for (auto& ch : node->children) genStmt(ch);
        break;

    default:
        break;
    }
}

void CodeGen::genStmtList(const ASTNodePtr& node)
{
    for (auto& st : node->children)
        genStmt(st);
}

// =====================================================
// VarDecl
// =====================================================
void CodeGen::genVarDecl(const ASTNodePtr& node)
{
    ASTNodePtr list = node->children[0];

    for (auto& item : list->children)
    {
        std::string varName = item->children[0]->token.lexeme;
        std::string typeName = item->children[1]->token.lexeme;

        llvm::Type* ty = mapType(typeName);
        if (!ty) continue;

        llvm::Value* alloc = Builder->CreateAlloca(ty, nullptr, varName);
        namedValues[varName] = alloc;

        // optional initializer: '=' Expr
        if (item->children.size() >= 3) {
            llvm::Value* initVal = genExpr(item->children[2]);
            if (initVal) storeVar(varName, initVal);
        }
    }
}

// =====================================================
// AssignOpStmt: ADD/SUB/MUL/DIV/MOD/INC/DEC/PLE/MIE/AND/OR
// children are Identifier leaves
// =====================================================
void CodeGen::genAssignOpStmt(const ASTNodePtr& node)
{
    const TokenType op = node->token.type;

    auto getName = [&](int i) -> std::string {
        if (i < 0 || i >= (int)node->children.size()) return "";
        return node->children[i]->token.lexeme;
    };

    std::string x = getName(0);
    if (x.empty()) return;

    if (op == TokenType::KW_INC || op == TokenType::KW_DEC) {
        llvm::Value* cur = loadVar(x);
        if (!cur) return;

        llvm::Value* one = nullptr;
        if (isInt(cur->getType()))
            one = llvm::ConstantInt::get(cur->getType(), 1);
        else if (isFloat(cur->getType()))
            one = llvm::ConstantFP::get(cur->getType(), 1.0);
        else
            return;

        llvm::Value* next = nullptr;
        if (op == TokenType::KW_INC) {
            next = isFloat(cur->getType()) ? Builder->CreateFAdd(cur, one)
                                           : Builder->CreateAdd(cur, one);
        } else {
            next = isFloat(cur->getType()) ? Builder->CreateFSub(cur, one)
                                           : Builder->CreateSub(cur, one);
        }

        storeVar(x, next);
        return;
    }

    std::string y = getName(1);
    if (y.empty()) return;

    if (op == TokenType::KW_PLE || op == TokenType::KW_MIE) {
        llvm::Value* cur = loadVar(x);
        llvm::Value* rhs = loadVar(y);
        if (!cur || !rhs) return;

        bool f = isFloat(cur->getType()) || isFloat(rhs->getType());
        if (f) {
            cur = implicitCast(cur, llvm::Type::getDoubleTy(*Ctx));
            rhs = implicitCast(rhs, llvm::Type::getDoubleTy(*Ctx));
            llvm::Value* next = (op == TokenType::KW_PLE) ? Builder->CreateFAdd(cur, rhs)
                                                          : Builder->CreateFSub(cur, rhs);
            storeVar(x, next);
        } else {
            cur = implicitCast(cur, llvm::Type::getInt32Ty(*Ctx));
            rhs = implicitCast(rhs, llvm::Type::getInt32Ty(*Ctx));
            llvm::Value* next = (op == TokenType::KW_PLE) ? Builder->CreateAdd(cur, rhs)
                                                          : Builder->CreateSub(cur, rhs);
            storeVar(x, next);
        }
        return;
    }

    std::string z = getName(2);
    if (z.empty()) return;

    llvm::Value* a = loadVar(y);
    llvm::Value* b = loadVar(z);
    if (!a || !b) return;

    if (op == TokenType::KW_AND || op == TokenType::KW_OR) {
        a = implicitCast(a, llvm::Type::getInt1Ty(*Ctx));
        b = implicitCast(b, llvm::Type::getInt1Ty(*Ctx));
        llvm::Value* r = (op == TokenType::KW_AND) ? Builder->CreateAnd(a, b)
                                                   : Builder->CreateOr(a, b);
        storeVar(x, r);
        return;
    }

    bool f = isFloat(a->getType()) || isFloat(b->getType());
    if (f) {
        a = implicitCast(a, llvm::Type::getDoubleTy(*Ctx));
        b = implicitCast(b, llvm::Type::getDoubleTy(*Ctx));

        llvm::Value* r = nullptr;
        if (op == TokenType::KW_ADD) r = Builder->CreateFAdd(a, b);
        else if (op == TokenType::KW_SUB) r = Builder->CreateFSub(a, b);
        else if (op == TokenType::KW_MUL) r = Builder->CreateFMul(a, b);
        else if (op == TokenType::KW_DIV) r = Builder->CreateFDiv(a, b);
        else if (op == TokenType::KW_MOD) return; // float % unsupported
        else return;

        storeVar(x, r);
    } else {
        a = implicitCast(a, llvm::Type::getInt32Ty(*Ctx));
        b = implicitCast(b, llvm::Type::getInt32Ty(*Ctx));

        llvm::Value* r = nullptr;
        if (op == TokenType::KW_ADD) r = Builder->CreateAdd(a, b);
        else if (op == TokenType::KW_SUB) r = Builder->CreateSub(a, b);
        else if (op == TokenType::KW_MUL) r = Builder->CreateMul(a, b);
        else if (op == TokenType::KW_DIV) r = Builder->CreateSDiv(a, b);
        else if (op == TokenType::KW_MOD) r = Builder->CreateSRem(a, b);
        else return;

        storeVar(x, r);
    }
}

// =====================================================
// PrintStmt
// =====================================================
void CodeGen::genPrint(const ASTNodePtr& node)
{
    llvm::Value* val = genExpr(node->children[0]);
    if (!val) return;

    llvm::Type* ty = val->getType();
    llvm::Function* fn = nullptr;

    if (isInt(ty)) {
        fn = Mod->getFunction("print_i32");
        if (!fn) {
            llvm::FunctionType* FT = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*Ctx),
                { llvm::Type::getInt32Ty(*Ctx) },
                false);
            fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "print_i32", Mod.get());
        }
        val = implicitCast(val, llvm::Type::getInt32Ty(*Ctx));
        Builder->CreateCall(fn, { val });
    }
    else if (isFloat(ty)) {
        fn = Mod->getFunction("print_f64");
        if (!fn) {
            llvm::FunctionType* FT = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*Ctx),
                { llvm::Type::getDoubleTy(*Ctx) },
                false);
            fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "print_f64", Mod.get());
        }
        val = implicitCast(val, llvm::Type::getDoubleTy(*Ctx));
        Builder->CreateCall(fn, { val });
    }
    else if (isBool(ty)) {
        fn = Mod->getFunction("print_bool");
        if (!fn) {
            llvm::FunctionType* FT = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*Ctx),
                { llvm::Type::getInt1Ty(*Ctx) },
                false);
            fn = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "print_bool", Mod.get());
        }
        val = implicitCast(val, llvm::Type::getInt1Ty(*Ctx));
        Builder->CreateCall(fn, { val });
    }
    else {
        std::cerr << "Unsupported type for print\n";
    }
}

// =====================================================
// Block
// =====================================================
void CodeGen::genBlock(const ASTNodePtr& node)
{
    genStmtList(node->children[0]);
}

// =====================================================
// IfStmt
// =====================================================
void CodeGen::genIf(const ASTNodePtr& node)
{
    llvm::Function* fn = Builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(*Ctx, "if.then", fn);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(*Ctx, "if.else", fn);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(*Ctx, "if.end",  fn);

    llvm::Value* cond = genExpr(node->children[0]);
    cond = implicitCast(cond, llvm::Type::getInt1Ty(*Ctx));

    Builder->CreateCondBr(cond, thenBB, elseBB);

    Builder->SetInsertPoint(thenBB);
    genStmt(node->children[1]);
    Builder->CreateBr(endBB);

    Builder->SetInsertPoint(elseBB);
    if (node->children.size() == 3)
        genStmt(node->children[2]);
    Builder->CreateBr(endBB);

    Builder->SetInsertPoint(endBB);
}

// =====================================================
// WhileStmt
// =====================================================
void CodeGen::genWhile(const ASTNodePtr& node)
{
    llvm::Function* fn = Builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*Ctx, "while.cond", fn);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Ctx, "while.body", fn);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(*Ctx, "while.end",  fn);

    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(condBB);
    llvm::Value* cond = genExpr(node->children[0]);
    cond = implicitCast(cond, llvm::Type::getInt1Ty(*Ctx));
    Builder->CreateCondBr(cond, bodyBB, endBB);

    Builder->SetInsertPoint(bodyBB);
    genStmt(node->children[1]);
    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(endBB);
}

// =====================================================
// ForStmt
// =====================================================
void CodeGen::genFor(const ASTNodePtr& node)
{
    // init (ForInit wraps: [id, arithExpr])
    genStmt(node->children[0]);

    llvm::Function* fn = Builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* condBB = llvm::BasicBlock::Create(*Ctx, "for.cond", fn);
    llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(*Ctx, "for.body", fn);
    llvm::BasicBlock* updBB  = llvm::BasicBlock::Create(*Ctx, "for.update", fn);
    llvm::BasicBlock* endBB  = llvm::BasicBlock::Create(*Ctx, "for.end",  fn);

    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(condBB);
    // ForCond node wraps BoolExpr as child[0]
    llvm::Value* cond = genExpr(node->children[1]->children[0]);
    cond = implicitCast(cond, llvm::Type::getInt1Ty(*Ctx));
    Builder->CreateCondBr(cond, bodyBB, endBB);

    Builder->SetInsertPoint(bodyBB);
    genStmt(node->children[3]);
    Builder->CreateBr(updBB);

    Builder->SetInsertPoint(updBB);
    genStmt(node->children[2]); // ForUpdate wraps AssignOpStmt
    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(endBB);
}

// =====================================================
// ForeachStmt (kept simple: no real array support yet)
// =====================================================
void CodeGen::genForeach(const ASTNodePtr& node)
{
    // if you don't have runtime/array IR support yet, just generate body
    genStmt(node->children[2]);
}

// =====================================================
// genExpr dispatcher
// =====================================================
llvm::Value* CodeGen::genExpr(const ASTNodePtr& node)
{
    switch (node->type)
    {
    case ASTNodeType::Expr:
        return genExpr(node->children[0]);

    case ASTNodeType::BoolExpr:  return genBoolExpr(node);
    case ASTNodeType::BoolOr:    return genBoolOr(node);
    case ASTNodeType::BoolAnd:   return genBoolAnd(node);
    case ASTNodeType::RelExpr:   return genRel(node);
    case ASTNodeType::ArithExpr: return genArith(node);
    case ASTNodeType::Term:      return genTerm(node);
    case ASTNodeType::Power:     return genPower(node);

    case ASTNodeType::IntLiteral:
    case ASTNodeType::FloatLiteral:
    case ASTNodeType::BoolLiteral:
    case ASTNodeType::Identifier:
    case ASTNodeType::Factor:
        return genFactor(node);

    default:
        std::cerr << "Unhandled expression type: " << (int)node->type << "\n";
        return llvm::ConstantInt::getFalse(*Ctx);
    }
}

llvm::Value* CodeGen::genBoolExpr(const ASTNodePtr& node)
{
    return genExpr(node->children[0]);
}

llvm::Value* CodeGen::genBoolOr(const ASTNodePtr& node)
{
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    L = implicitCast(L, llvm::Type::getInt1Ty(*Ctx));
    R = implicitCast(R, llvm::Type::getInt1Ty(*Ctx));
    return Builder->CreateOr(L, R);
}

llvm::Value* CodeGen::genBoolAnd(const ASTNodePtr& node)
{
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    L = implicitCast(L, llvm::Type::getInt1Ty(*Ctx));
    R = implicitCast(R, llvm::Type::getInt1Ty(*Ctx));
    return Builder->CreateAnd(L, R);
}

// =====================================================
// RelExpr
// =====================================================
llvm::Value* CodeGen::genRel(const ASTNodePtr& node)
{
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);

    llvm::Type* TL = L->getType();
    llvm::Type* TR = R->getType();

    std::string op = node->token.lexeme;

    if (isFloat(TL) || isFloat(TR)) {
        L = implicitCast(L, llvm::Type::getDoubleTy(*Ctx));
        R = implicitCast(R, llvm::Type::getDoubleTy(*Ctx));

        if (op == "<")  return Builder->CreateFCmpOLT(L, R);
        if (op == ">")  return Builder->CreateFCmpOGT(L, R);
        if (op == "<=") return Builder->CreateFCmpOLE(L, R);
        if (op == ">=") return Builder->CreateFCmpOGE(L, R);
        if (op == "==") return Builder->CreateFCmpOEQ(L, R);
        if (op == "!=") return Builder->CreateFCmpONE(L, R);
    } else {
        L = implicitCast(L, llvm::Type::getInt32Ty(*Ctx));
        R = implicitCast(R, llvm::Type::getInt32Ty(*Ctx));

        if (op == "<")  return Builder->CreateICmpSLT(L, R);
        if (op == ">")  return Builder->CreateICmpSGT(L, R);
        if (op == "<=") return Builder->CreateICmpSLE(L, R);
        if (op == ">=") return Builder->CreateICmpSGE(L, R);
        if (op == "==") return Builder->CreateICmpEQ(L, R);
        if (op == "!=") return Builder->CreateICmpNE(L, R);
    }

    std::cerr << "RelExpr: unsupported\n";
    return llvm::ConstantInt::getFalse(*Ctx);
}

// =====================================================
// ArithExpr: + , -
// =====================================================
llvm::Value* CodeGen::genArith(const ASTNodePtr& node)
{
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    std::string op = node->token.lexeme;

    bool f = isFloat(L->getType()) || isFloat(R->getType());
    if (f) {
        L = implicitCast(L, llvm::Type::getDoubleTy(*Ctx));
        R = implicitCast(R, llvm::Type::getDoubleTy(*Ctx));
        if (op == "+") return Builder->CreateFAdd(L, R);
        else          return Builder->CreateFSub(L, R);
    } else {
        L = implicitCast(L, llvm::Type::getInt32Ty(*Ctx));
        R = implicitCast(R, llvm::Type::getInt32Ty(*Ctx));
        if (op == "+") return Builder->CreateAdd(L, R);
        else          return Builder->CreateSub(L, R);
    }
}

// =====================================================
// Term: *, /, %
// =====================================================
llvm::Value* CodeGen::genTerm(const ASTNodePtr& node)
{
    llvm::Value* L = genExpr(node->children[0]);
    llvm::Value* R = genExpr(node->children[1]);
    std::string op = node->token.lexeme;

    bool f = isFloat(L->getType()) || isFloat(R->getType());
    if (f) {
        L = implicitCast(L, llvm::Type::getDoubleTy(*Ctx));
        R = implicitCast(R, llvm::Type::getDoubleTy(*Ctx));
        if (op == "*") return Builder->CreateFMul(L, R);
        if (op == "/") return Builder->CreateFDiv(L, R);
        std::cerr << "float % unsupported\n";
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*Ctx), 0.0);
    } else {
        L = implicitCast(L, llvm::Type::getInt32Ty(*Ctx));
        R = implicitCast(R, llvm::Type::getInt32Ty(*Ctx));
        if (op == "*") return Builder->CreateMul(L, R);
        if (op == "/") return Builder->CreateSDiv(L, R);
        if (op == "%") return Builder->CreateSRem(L, R);
        std::cerr << "Term: unknown op\n";
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 0);
    }
}

// =====================================================
// Power
// =====================================================
llvm::Value* CodeGen::genPower(const ASTNodePtr& node)
{
    llvm::Value* base = implicitCast(genExpr(node->children[0]), llvm::Type::getInt32Ty(*Ctx));
    llvm::Value* exp  = implicitCast(genExpr(node->children[1]), llvm::Type::getInt32Ty(*Ctx));

    llvm::Value* resultAlloca = Builder->CreateAlloca(llvm::Type::getInt32Ty(*Ctx));
    llvm::Value* counterAlloca = Builder->CreateAlloca(llvm::Type::getInt32Ty(*Ctx));

    Builder->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 1), resultAlloca);
    Builder->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 0), counterAlloca);

    llvm::Function* fn = Builder->GetInsertBlock()->getParent();

    llvm::BasicBlock* condBB  = llvm::BasicBlock::Create(*Ctx, "pow.cond", fn);
    llvm::BasicBlock* bodyBB  = llvm::BasicBlock::Create(*Ctx, "pow.body", fn);
    llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(*Ctx, "pow.end",  fn);

    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(condBB);
    llvm::Value* c = Builder->CreateLoad(llvm::Type::getInt32Ty(*Ctx), counterAlloca);
    llvm::Value* cond = Builder->CreateICmpSLT(c, exp);
    Builder->CreateCondBr(cond, bodyBB, afterBB);

    Builder->SetInsertPoint(bodyBB);
    llvm::Value* r = Builder->CreateLoad(llvm::Type::getInt32Ty(*Ctx), resultAlloca);
    llvm::Value* mul = Builder->CreateMul(r, base);
    Builder->CreateStore(mul, resultAlloca);

    llvm::Value* c2 = Builder->CreateAdd(c, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), 1));
    Builder->CreateStore(c2, counterAlloca);
    Builder->CreateBr(condBB);

    Builder->SetInsertPoint(afterBB);
    return Builder->CreateLoad(llvm::Type::getInt32Ty(*Ctx), resultAlloca);
}

// =====================================================
// Factor
// =====================================================
llvm::Value* CodeGen::genFactor(const ASTNodePtr& node)
{
    switch (node->type)
    {
    case ASTNodeType::IntLiteral:
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(*Ctx), std::stoi(node->token.lexeme));

    case ASTNodeType::FloatLiteral:
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*Ctx), std::stod(node->token.lexeme));

    case ASTNodeType::BoolLiteral:
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(*Ctx), node->token.lexeme == "true" ? 1 : 0);

    case ASTNodeType::Identifier:
        return loadVar(node->token.lexeme);

    case ASTNodeType::Factor:
        if (!node->children.empty())
            return genExpr(node->children[0]);
        break;

    default:
        break;
    }

    std::cerr << "Factor: unsupported\n";
    return llvm::ConstantInt::getFalse(*Ctx);
}

// =====================================================
// saveIRToFile / emitObjectFile / runMain (unchanged)
// =====================================================
bool CodeGen::saveIRToFile(const std::string& path) const
{
    if (!Mod) {
        std::cerr << "ERROR: Module is null, cannot save IR.\n";
        return false;
    }

    std::error_code EC;
    llvm::raw_fd_ostream out(path, EC, llvm::sys::fs::OF_Text);
    if (EC) {
        std::cerr << "ERROR: cannot open IR file '" << path
                  << "': " << EC.message() << "\n";
        return false;
    }

    Mod->print(out, nullptr);
    return true;
}

bool CodeGen::emitObjectFile(const std::string& path) const
{
    if (!Mod) {
        std::cerr << "ERROR: Module is null, cannot emit object file.\n";
        return false;
    }

    std::string triple = llvm::sys::getDefaultTargetTriple();
    Mod->setTargetTriple(triple);

    std::string error;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, error);

    if (!target) {
        std::cerr << "ERROR: " << error << "\n";
        return false;
    }

    std::string cpu = "generic";
    std::string features = "";
    llvm::TargetOptions opt;
    auto RM = llvm::Optional<llvm::Reloc::Model>();

    std::unique_ptr<llvm::TargetMachine> TM(
        target->createTargetMachine(triple, cpu, features, opt, RM));

    Mod->setDataLayout(TM->createDataLayout());

    std::error_code EC;
    llvm::raw_fd_ostream dest(path, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "ERROR: cannot open object file '" << path
                  << "': " << EC.message() << "\n";
        return false;
    }

    llvm::legacy::PassManager pass;
    if (TM->addPassesToEmitFile(pass, dest, nullptr, llvm::CGFT_ObjectFile)) {
        std::cerr << "ERROR: TargetMachine can't emit a file of this type.\n";
        return false;
    }

    pass.run(*Mod);
    dest.flush();

    return true;
}

int CodeGen::runMain()
{
    if (!MainFn) {
        std::cerr << "ERROR: MainFn is null, did compile() fail?\n";
        return 1;
    }

    std::string errStr;

    std::unique_ptr<llvm::Module> M = std::move(Mod);
    if (!M) {
        std::cerr << "ERROR: Module already consumed or null.\n";
        return 1;
    }

    llvm::EngineBuilder builder(std::move(M));
    builder.setErrorStr(&errStr);
    builder.setEngineKind(llvm::EngineKind::JIT);

    llvm::ExecutionEngine* EE = builder.create();
    if (!EE) {
        std::cerr << "Failed to create ExecutionEngine: " << errStr << "\n";
        return 1;
    }

    std::vector<llvm::GenericValue> noArgs;
    llvm::GenericValue ret = EE->runFunction(MainFn, noArgs);

    return static_cast<int>(ret.IntVal.getSExtValue());
}
