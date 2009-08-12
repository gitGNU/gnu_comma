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

    // Get the llvm function for this routine.
    SRFn = getOrCreateSubroutineDeclaration(srDecl);

    // Resolve the defining declaration, if needed.
    if (SRDecl->getDefiningDeclaration())
        SRDecl = SRDecl->getDefiningDeclaration();

    injectSubroutineArgs();
    emitSubroutineBody();
    llvm::verifyFunction(*SRFn);
}

/// Generates code for the current subroutines body.
void CodeGenRoutine::emitSubroutineBody()
{
    // Create the return block.
    returnBB = llvm::BasicBlock::Create("return", SRFn);

    // Create the entry block and set it as the current insertion point for our
    // IRBuilder.
    entryBB = llvm::BasicBlock::Create("entry", SRFn, returnBB);
    Builder.SetInsertPoint(entryBB);

    // If we are generating a function, allocate a stack slot for the return
    // value.
    if (isa<FunctionDecl>(SRDecl))
        returnValue = Builder.CreateAlloca(SRFn->getReturnType());

    // Codegen the function body.  If the resulting insertion context is not
    // properly terminated, create a branch to the return BB.
    llvm::BasicBlock *bodyBB = emitBlockStmt(SRDecl->getBody(), entryBB);
    if (!Builder.GetInsertBlock()->getTerminator())
        Builder.CreateBr(returnBB);

    emitPrologue(bodyBB);
    emitEpilogue();
}

/// Given the current SubroutineDecl and llvm::Function, initialize
/// CodeGenRoutine::percent with the llvm value corresponding to the first
/// (implicit) argument.  Also, name the llvm arguments after the source
/// formals, and populate the lookup tables such that a search for a parameter
/// decl yields the corresponding llvm value.
void CodeGenRoutine::injectSubroutineArgs()
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
        // FIXME: This is a hack.  We rely here on the esoteric property that a
        // local decl is declared in an "add" context.  Rather, check that the
        // decl originates from the curent domain, or explicity tag decls as
        // local in the AST.
        const DeclRegion *region = decl->getDeclRegion();
        return isa<AddDecl>(region);
    }
    return false;
}

bool CodeGenRoutine::isLocalCall(const ProcedureCallStmt *stmt)
{
    // FIXME: This is a hack.  We rely here on the esoteric property that a
    // local decl is declared in an "add" context.  Rather, check that the decl
    // originates from the curent domain, or explicity tag decls as local in the
    // AST.
    const ProcedureDecl *pdecl = stmt->getConnective();
    const DeclRegion *region = pdecl->getDeclRegion();
    return isa<AddDecl>(region);
}

void CodeGenRoutine::emitObjectDecl(ObjectDecl *objDecl)
{
    llvm::Value *stackSlot = createStackSlot(objDecl);

    if (objDecl->hasInitializer()) {
        llvm::Value *init = emitExpr(objDecl->getInitializer());
        Builder.CreateStore(init, stackSlot);
    }
}

llvm::Value *CodeGenRoutine::emitScalarLoad(llvm::Value *ptr)
{
    return Builder.CreateLoad(ptr);
}

llvm::Value *CodeGenRoutine::getStackSlot(Decl *decl)
{
    // FIXME: We should be more precise.  Subroutine parameters, for example,
    // are accessible via lookupDecl.  There is no way (ATM) to distinguish a
    // parameter from a alloca'd local.  The lookup methods should provide finer
    // grained resolution.
    llvm::Value *stackSlot = lookupDecl(decl);
    assert(stackSlot && "Lookup failed!");
    return stackSlot;
}

llvm::Value *CodeGenRoutine::createStackSlot(Decl *decl)
{
    assert(lookupDecl(decl) == 0 &&
           "Cannot create stack slot for preexisting decl!");

    // The only kind of declaration we emit stack slots for are value
    // declarations, hense the cast.
    ValueDecl *vDecl = cast<ValueDecl>(decl);
    const llvm::Type *type = CGTypes.lowerType(vDecl->getType());
    llvm::BasicBlock *savedBB = Builder.GetInsertBlock();

    Builder.SetInsertPoint(entryBB);
    llvm::Value *stackSlot = Builder.CreateAlloca(type);
    declTable[decl] = stackSlot;
    Builder.SetInsertPoint(savedBB);
    return stackSlot;
}

llvm::Value *CodeGenRoutine::getOrCreateStackSlot(Decl *decl)
{
    if (llvm::Value *stackSlot = lookupDecl(decl))
        return stackSlot;
    else
        return createStackSlot(decl);
}

llvm::Value *CodeGenRoutine::emitVariableReference(Expr *expr)
{
    // FIXME: All variable references must be DeclRefExpressions for now.  This
    // will not always be the case.
    DeclRefExpr *refExpr = cast<DeclRefExpr>(expr);
    Decl *refDecl = refExpr->getDeclaration();

    if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
        PM::ParameterMode paramMode = pvDecl->getParameterMode();

        // Enusure that the parameter has a mode consistent with reference
        // emission.
        assert((paramMode == PM::MODE_OUT or paramMode == PM::MODE_IN_OUT) &&
               "Cannot take reference to a parameter with mode IN!");

        return lookupDecl(pvDecl);
    }

    if (ObjectDecl *objDecl = dyn_cast<ObjectDecl>(refDecl)) {
        // Local object declarations are always generated with repect to a stack
        // slot.
        return getStackSlot(objDecl);
    }

    assert(false && "Cannot codegen reference for expression!");
}

llvm::Value *CodeGenRoutine::emitValue(Expr *expr)
{
    if (DeclRefExpr *refExpr = dyn_cast<DeclRefExpr>(expr)) {
        Decl *refDecl = refExpr->getDeclaration();

        if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            // If the parameter mode is either "out" or "in out" then load the
            // actual value.
            llvm::Value *param = lookupDecl(pvDecl);
            PM::ParameterMode paramMode = pvDecl->getParameterMode();

            if (paramMode == PM::MODE_OUT or paramMode == PM::MODE_IN_OUT)
                return Builder.CreateLoad(param);
            else
                return param;
        }

        // Otherwise, we must have an ObjectDecl.  Just load from the alloca'd
        // stack slot.
        ObjectDecl *objDecl = cast<ObjectDecl>(refDecl);
        llvm::Value *stackSlot = lookupDecl(objDecl);
        return Builder.CreateLoad(stackSlot);
    }

    // FIXME:  This is not precise enough, but works for the remaining cases.
    return emitExpr(expr);
}
