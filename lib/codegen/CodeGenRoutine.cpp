//===-- codegen/CodeGenRoutine.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/CodeGenCapsule.h"
#include "comma/codegen/CodeGenRoutine.h"
#include "comma/codegen/CodeGenTypes.h"

#include "llvm/Analysis/Verifier.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

CodeGenRoutine::CodeGenRoutine(CodeGenCapsule &CGC)
    : CGC(CGC),
      CG(CGC.getCodeGen()),
      CGTypes(CG.getTypeGenerator()),
      CRT(CG.getRuntime()),
      SRDecl(0),
      SRFn(0),
      entryBB(0),
      returnBB(0),
      returnValue(0) { }

void CodeGenRoutine::declareSubroutine(SubroutineDecl *srDecl)
{
    getOrCreateSubroutineDeclaration(srDecl);
}

llvm::Function *
CodeGenRoutine::getOrCreateSubroutineDeclaration(SubroutineDecl *srDecl)
{
    const llvm::FunctionType *srTy = CGTypes.lowerType(srDecl->getType());
    std::string srName = CG.getLinkName(srDecl);

    llvm::Function *fn =
        dyn_cast_or_null<llvm::Function>(CG.lookupGlobal(srName));

    if (!fn) {
        fn = CG.makeFunction(srTy, srName);
        CG.insertGlobal(srName, fn);
    }

    return fn;
}

void CodeGenRoutine::emitSubroutine(SubroutineDecl *srDecl)
{
    // Remember the declaration we are processing.
    SRDecl = srDecl;

    const llvm::FunctionType *SRTy = CGTypes.lowerType(SRDecl->getType());
    std::string SRName = CG.getLinkName(SRDecl);

    // Get the llvm function for this routine.
    SRFn = getOrCreateSubroutineDeclaration(srDecl);

    // Resolve the defining declaration, if needed.
    if (SRDecl->getDefiningDeclaration())
        SRDecl = SRDecl->getDefiningDeclaration();

    {
        // Extract and save the first implicit argument "%".
        llvm::Function::arg_iterator iter = SRFn->arg_begin();
        percent = iter++;

        // For each formal argument, set its name to match that of the declaration.
        // Also, populate the declTable with entries for each of the parameters.
        for (unsigned i = 0; iter != SRFn->arg_end(); ++iter, ++i) {
            ParamValueDecl *param = SRDecl->getParam(i);
            iter->setName(param->getString());
            declTable[param] = iter;
        }
    }

    // Create the return block.
    returnBB = llvm::BasicBlock::Create("return", SRFn);

    // Create the entry block (before the return block) and set it as the
    // current insertion point for our IRBuilder.
    entryBB = llvm::BasicBlock::Create("entry", SRFn, returnBB);
    Builder.SetInsertPoint(entryBB);

    // If we are generating a function, allocate a stack slot for the return
    // value.
    if (isa<FunctionDecl>(SRDecl))
        returnValue = Builder.CreateAlloca(SRTy->getReturnType());

    // Codegen the function body.  If the insertion context is not properly
    // terminated, create a branch to the return BB.
    llvm::BasicBlock *bodyBB = emitBlockStmt(SRDecl->getBody(), entryBB);
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(returnBB);

    emitPrologue(bodyBB);
    emitEpilogue();
    llvm::verifyFunction(*SRFn);
}

void CodeGenRoutine::emitPrologue(llvm::BasicBlock *bodyBB)
{
    // FIXME:  In the future, we will have accumulated much state about what the
    // prologue should contain.  For now, just branch to the entry block for the
    // body.
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    Builder.CreateBr(bodyBB);
    Builder.SetInsertPoint(savedBB);
}

void CodeGenRoutine::emitEpilogue()
{
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(returnBB);
    if (returnValue) {
        llvm::Value *V = Builder.CreateLoad(returnValue);
        Builder.CreateRet(V);
    }
    else
        Builder.CreateRetVoid();
    Builder.SetInsertPoint(savedBB);
}

llvm::Value *CodeGenRoutine::lookupDecl(Decl *decl)
{
    DeclMap::iterator iter = declTable.find(decl);

    if (iter != declTable.end())
        return iter->second;
    return 0;
}

bool CodeGenRoutine::isDirectCall(const FunctionCallExpr *expr)
{
    const FunctionDecl *decl = dyn_cast<FunctionDecl>(expr->getConnective());
    if (decl) {
        const DeclRegion *region = decl->getDeclRegion();
        return isa<DomainInstanceDecl>(region);
    }
    return false;
}

bool CodeGenRoutine::isDirectCall(const ProcedureCallStmt *stmt)
{
    const ProcedureDecl *pdecl = stmt->getConnective();
    const DeclRegion *region = pdecl->getDeclRegion();
    return isa<DomainInstanceDecl>(region);
}

bool CodeGenRoutine::isLocalCall(const FunctionCallExpr *expr)
{
    const FunctionDecl *decl = dyn_cast<FunctionDecl>(expr->getConnective());
    if (decl) {
        // FIXME: This is a hack (and not truely correct).  We rely here on the
        // esoteric property that a local decl is declared in an "add" context.
        // Rather, check that the decl originates from the curent domain, or
        // explicity tag decls as local.
        const DeclRegion *region = decl->getDeclRegion();
        return isa<AddDecl>(region);
    }
    return false;
}

bool CodeGenRoutine::isLocalCall(const ProcedureCallStmt *stmt)
{
    // FIXME: This is a hack (and not truely correct).  We rely here on the
    // esoteric property that a local decl is declared in an "add" context.
    // Rather, check that the decl originates from the curent domain, or
    // explicity tag decls as local.
    const ProcedureDecl *pdecl = stmt->getConnective();
    const DeclRegion *region = pdecl->getDeclRegion();
    return isa<AddDecl>(region);
}

