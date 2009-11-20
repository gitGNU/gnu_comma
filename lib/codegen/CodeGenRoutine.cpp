//===-- codegen/CodeGenRoutine.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenCapsule.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "SRInfo.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/Mangle.h"

#include "llvm/Analysis/Verifier.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

CodeGenRoutine::CodeGenRoutine(CodeGenCapsule &CGC, SRInfo *info)
    : CG(CGC.getCodeGen()),
      CGC(CGC),
      CGT(CGC.getTypeGenerator()),
      CRT(CG.getRuntime()),
      SRI(info),
      Builder(CG.getLLVMContext()),
      SRF(0) { }

void CodeGenRoutine::emit()
{
    // If this declaration is imported (pragma import as completion), we are
    // done.
    if (SRI->isImported())
        return;

    // We need to codegen this subroutine.  Obtain a frame.
    std::auto_ptr<SRFrame> SRFHandle(new SRFrame(SRI, Builder));
    SRF = SRFHandle.get();
    emitSubroutineBody();
    llvm::verifyFunction(*SRI->getLLVMFunction());
}

void CodeGenRoutine::emitSubroutineBody()
{
    // Resolve the completion for this subroutine, if needed.
    SubroutineDecl *SRDecl = SRI->getDeclaration();
    if (SRDecl->getDefiningDeclaration())
        SRDecl = SRDecl->getDefiningDeclaration();

    // Codegen the function body.  If the resulting insertion context is not
    // properly terminated, create a branch to the return BB.
    BlockStmt *body = SRDecl->getBody();
    llvm::BasicBlock *bodyBB = emitBlockStmt(body, 0);
    if (!Builder.GetInsertBlock()->getTerminator())
        SRF->emitReturn();

    SRF->emitPrologue(bodyBB);
    SRF->emitEpilogue();
}

llvm::Function *CodeGenRoutine::getLLVMFunction() const
{
    return SRI->getLLVMFunction();
}

bool CodeGenRoutine::isDirectCall(const SubroutineCall *call)
{
    if (call->isAmbiguous())
        return false;

    const SubroutineDecl *decl = call->getConnective();
    const DeclRegion *region = decl->getDeclRegion();
    return isa<DomainInstanceDecl>(region);
}

bool CodeGenRoutine::isLocalCall(const SubroutineCall *call)
{
    if (call->isAmbiguous())
        return false;

    // FIXME: This is a hack.  We rely here on the esoteric property that a
    // local decl is declared in an "add" context.  Rather, check that the decl
    // originates from the curent domain, or explicity tag decls as local in the
    // AST.
    const SubroutineDecl *decl = call->getConnective();
    const DeclRegion *region = decl->getDeclRegion();
    return isa<AddDecl>(region);
}

bool CodeGenRoutine::isForeignCall(const SubroutineCall *call)
{
    if (call->isAmbiguous())
        return false;

    const SubroutineDecl *srDecl = call->getConnective();
    return srDecl->hasPragma(pragma::Import);
}

void CodeGenRoutine::emitObjectDecl(ObjectDecl *objDecl)
{
    Type *objTy = objDecl->getType();
    const llvm::Type *loweredTy = CGT.lowerType(objTy);

    if (ArrayType *arrTy = dyn_cast<ArrayType>(objTy)) {
        BoundsEmitter emitter(*this);
        const llvm::Type *boundTy = CGT.lowerArrayBounds(arrTy);
        llvm::Value *bounds = 0;
        llvm::Value *slot = 0;

        if (arrTy->isStaticallyConstrained())
            slot = SRF->createEntry(objDecl, activation::Slot, loweredTy);

        if (objDecl->hasInitializer()) {
            Expr *init = objDecl->getInitializer();
            if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(init)) {
                if (arrTy->isStaticallyConstrained()) {
                    // Perform the function call and add the destination to the
                    // argument set.
                    emitCompositeCall(call, slot);

                    // Synthesize bounds for this declaration.
                    bounds =
                        SRF->createEntry(objDecl, activation::Bounds, boundTy);
                    emitter.synthStaticArrayBounds(Builder, arrTy, bounds);
                }
                else {
                    // FIXME: Checks are needed when the initializer is
                    // unconstrained but the declaration is.  However, this
                    // currently cannot happen, hence the assert.
                    assert(slot == 0);
                    std::pair<llvm::Value*, llvm::Value*> result;
                    result = emitVStackCall(call);
                    bounds = result.second;
                    SRF->associate(objDecl, activation::Slot, result.first);
                    SRF->associate(objDecl, activation::Bounds, bounds);
                }
            }
            else {
                std::pair<llvm::Value*, llvm::Value*> result;
                result = emitArrayExpr(init, slot, true);
                if (!slot)
                    SRF->associate(objDecl, activation::Slot, result.first);
                if (!bounds)
                    bounds =
                        SRF->createEntry(objDecl, activation::Bounds, boundTy);
                Builder.CreateStore(result.second, bounds);
            }
        }
        else {
            bounds = SRF->createEntry(objDecl, activation::Bounds, boundTy);
            emitter.synthStaticArrayBounds(Builder, arrTy, bounds);
        }
        return;
    }

    // Otherwise, this is a simple non-composite type.  Allocate a stack slot
    // and evaluate the initializer if present.
    llvm::Value *slot = SRF->createEntry(objDecl, activation::Slot, loweredTy);
    if (objDecl->hasInitializer()) {
        llvm::Value *value = emitValue(objDecl->getInitializer());
        Builder.CreateStore(value, slot);
    }
}

llvm::Value *CodeGenRoutine::emitVariableReference(Expr *expr)
{
    if (DeclRefExpr *refExpr = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *refDecl = refExpr->getDeclaration();
        llvm::Value *addr = 0;

        if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
            // Ensure that the parameter has a mode consistent with reference
            // emission.
            PM::ParameterMode paramMode = pvDecl->getParameterMode();
            assert((paramMode == PM::MODE_OUT || paramMode == PM::MODE_IN_OUT)
                   && "Cannot take reference to a parameter with mode IN!");
            addr = SRF->lookup(pvDecl, activation::Slot);
        }
        else {
            // Otherwise, we must have a local object declaration.  Simply
            // return the associated stack slot.
            ObjectDecl *objDecl = cast<ObjectDecl>(refDecl);
            addr = SRF->lookup(objDecl, activation::Slot);
        }
        return addr;
    }
    else if (IndexedArrayExpr *idxExpr = dyn_cast<IndexedArrayExpr>(expr))
        return emitIndexedArrayRef(idxExpr);

    assert(false && "Cannot codegen reference for expression!");
    return 0;
}

llvm::Value *CodeGenRoutine::emitValue(Expr *expr)
{
    switch (expr->getKind()) {

    default:
        if (AttribExpr *attrib = dyn_cast<AttribExpr>(expr))
            return emitAttribExpr(attrib);
        else
            assert(false && "Cannot codegen expression!");
        break;

    case Ast::AST_DeclRefExpr:
        return emitDeclRefExpr(cast<DeclRefExpr>(expr));

    case Ast::AST_FunctionCallExpr:
        return emitSimpleCall(cast<FunctionCallExpr>(expr));

    case Ast::AST_InjExpr:
        return emitInjExpr(cast<InjExpr>(expr));

    case Ast::AST_PrjExpr:
        return emitPrjExpr(cast<PrjExpr>(expr));

    case Ast::AST_IntegerLiteral:
        return emitIntegerLiteral(cast<IntegerLiteral>(expr));

    case Ast::AST_IndexedArrayExpr:
        return emitIndexedArrayValue(cast<IndexedArrayExpr>(expr));

    case Ast::AST_ConversionExpr:
        return emitConversionValue(cast<ConversionExpr>(expr));
    }
}

void CodeGenRoutine::emitPragmaAssert(PragmaAssert *pragma)
{
    llvm::Value *condition = emitValue(pragma->getCondition());
    llvm::GlobalVariable *msgVar = CG.emitInternString(pragma->getMessage());
    llvm::Value *message =
        CG.getPointerCast(msgVar, CG.getPointerType(CG.getInt8Ty()));

    // Create basic blocks for when the assertion fires and another for the
    // continuation.
    llvm::BasicBlock *assertBB = makeBasicBlock("assert-fail");
    llvm::BasicBlock *passBB = makeBasicBlock("assert-pass");

    // If the condition is true, the assertion does not fire.
    Builder.CreateCondBr(condition, passBB, assertBB);

    // Generate the call to _comma_assert_fail.
    Builder.SetInsertPoint(assertBB);
    CRT.assertFail(Builder, message);

    // Switch to the continuation block.
    Builder.SetInsertPoint(passBB);
}

//===----------------------------------------------------------------------===//
// LLVM IR generation helpers.

llvm::BasicBlock *
CodeGenRoutine::makeBasicBlock(const std::string &name,
                               llvm::BasicBlock *insertBefore) const
{
    return CG.makeBasicBlock(name, getLLVMFunction(), insertBefore);
}
