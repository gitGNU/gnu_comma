//===-- codegen/CodeGenRoutine.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CGContext.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "SRInfo.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"
#include "comma/codegen/Mangle.h"

#include "llvm/Analysis/Verifier.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

CodeGenRoutine::CodeGenRoutine(CGContext &CGC, SRInfo *info)
    : CG(CGC.getCG()),
      CGC(CGC),
      CGT(CGC.getCGT()),
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
    std::auto_ptr<SRFrame> SRFHandle(new SRFrame(SRI, *this, Builder));
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

void CodeGenRoutine::emitObjectDecl(ObjectDecl *objDecl)
{
    Type *objTy = resolveType(objDecl->getType());

    if (objTy->isCompositeType()) {
        emitCompositeObjectDecl(objDecl);
        return;
    }

    if (objTy->isFatAccessType()) {
        // If we have an initializer simply emit it and associate the temporary
        // with the object.  Otherwise allocate a slot of the appropritate fat
        // pointer structure and initialize the embedded pointer to null.
        if (objDecl->hasInitializer()) {
            CValue value = emitValue(objDecl->getInitializer());
            SRF->associate(objDecl, activation::Slot, value.first());
            return;
        }

        const llvm::StructType *fatTy;
        const llvm::PointerType *dataTy;
        llvm::Value *slot;
        llvm::Value *ptr;
        llvm::Value *null;

        fatTy = CGT.lowerFatAccessType(cast<AccessType>(objTy));
        dataTy = cast<llvm::PointerType>(fatTy->getElementType(0));
        slot = SRF->createEntry(objDecl, activation::Slot, fatTy);
        ptr = Builder.CreateStructGEP(slot, 0);
        null = llvm::ConstantPointerNull::get(dataTy);
        Builder.CreateStore(null, ptr);
        return;
    }

    // Otherwise, this is a simple non-composite type.  Allocate a stack slot
    // and evaluate the initializer if present.
    const llvm::Type *lowTy = CGT.lowerType(objTy);
    llvm::Value *slot = SRF->createEntry(objDecl, activation::Slot, lowTy);
    if (objDecl->hasInitializer()) {
        CValue value = emitValue(objDecl->getInitializer());
        Builder.CreateStore(value.first(), slot);
    }
}

void CodeGenRoutine::emitRenamedObjectDecl(RenamedObjectDecl *objDecl)
{
    Type *objTy = resolveType(objDecl->getType());
    Expr *objExpr = objDecl->getRenamedExpr()->ignoreInjPrj();
    llvm::Value *objValue;
    llvm::Value *objBounds;

    // For DecRefExpr's the target of the rename has already been evaluated.
    // Simply equate the declarations.
    if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(objExpr)) {
        ValueDecl *decl = DRE->getDeclaration();

        objValue = SRF->lookup(decl, activation::Slot);
        SRF->associate(objDecl, activation::Slot, objValue);

        if (objTy->isCompositeType()) {
            objBounds = SRF->lookup(decl, activation::Bounds);
            SRF->associate(objDecl, activation::Bounds, objBounds);
        }
        return;
    }

    // Otherwise evaluate the target as a reference if possible.
    if (objTy->isCompositeType()) {
        CValue result = emitCompositeExpr(objExpr, 0, false);
        objValue = result.first();
        objBounds = result.second();

        SRF->associate(objDecl, activation::Slot, objValue);
        SRF->associate(objDecl, activation::Bounds, objBounds);
        return;
    }

    objValue = emitReference(objExpr).first();
    SRF->associate(objDecl, activation::Slot, objValue);
}

CValue CodeGenRoutine::emitReference(Expr *expr)
{
    // Remove any outer inj and prj expressions.
    expr = expr->ignoreInjPrj();

    CValue result;

    // The most common case is a reference to a declaration.
    if (DeclRefExpr *refExpr = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = refExpr->getDeclaration();
        result = CValue::get(SRF->lookup(decl, activation::Slot));
    }
    else if (IndexedArrayExpr *idxExpr = dyn_cast<IndexedArrayExpr>(expr))
        result = emitIndexedArrayRef(idxExpr);
    else if (SelectedExpr *selExpr = dyn_cast<SelectedExpr>(expr))
        result = emitSelectedRef(selExpr);
    else if (DereferenceExpr *derefExpr = dyn_cast<DereferenceExpr>(expr)) {
        // Do not dereference, just return the pointer prefix.
        result = emitValue(derefExpr->getPrefix());
    }

    return result;
}

CValue CodeGenRoutine::emitValue(Expr *expr)
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
        return emitFunctionCall(cast<FunctionCallExpr>(expr));

    case Ast::AST_DereferenceExpr:
        return emitDereferencedValue(cast<DereferenceExpr>(expr));

    case Ast::AST_InjExpr:
        return emitInjExpr(cast<InjExpr>(expr));

    case Ast::AST_PrjExpr:
        return emitPrjExpr(cast<PrjExpr>(expr));

    case Ast::AST_IntegerLiteral:
        return emitIntegerLiteral(cast<IntegerLiteral>(expr));

    case Ast::AST_IndexedArrayExpr:
        return emitIndexedArrayValue(cast<IndexedArrayExpr>(expr));

    case Ast::AST_SelectedExpr:
        return emitSelectedValue(cast<SelectedExpr>(expr));

    case Ast::AST_ConversionExpr:
        return emitConversionValue(cast<ConversionExpr>(expr));

    case Ast::AST_NullExpr:
        return emitNullExpr(cast<NullExpr>(expr));

    case Ast::AST_QualifiedExpr:
        return emitValue(cast<QualifiedExpr>(expr)->getOperand());

    case Ast::AST_AllocatorExpr:
        return emitAllocatorValue(cast<AllocatorExpr>(expr));

    case Ast::AST_DiamondExpr:
        return emitDefaultValue(expr->getType());
    }

    return CValue::get(0);
}

void CodeGenRoutine::emitPragmaAssert(PragmaAssert *pragma)
{
    CValue condition = emitValue(pragma->getCondition());
    const llvm::PointerType *messageTy = CG.getInt8PtrTy();

    // Create basic blocks for when the assertion fires and another for the
    // continuation.
    llvm::BasicBlock *assertBB = SRF->makeBasicBlock("assert-fail");
    llvm::BasicBlock *passBB = SRF->makeBasicBlock("assert-pass");

    // If the condition is true, the assertion does not fire.
    Builder.CreateCondBr(condition.first(), passBB, assertBB);

    // Otherwise, evaluate the message if present and raise an Assert_Error
    // exception.
    Builder.SetInsertPoint(assertBB);
    llvm::Value *message;
    llvm::Value *messageLength;

    if (pragma->hasMessage()) {
        BoundsEmitter emitter(*this);
        CValue msg = emitCompositeExpr(pragma->getMessage(), 0, true);
        message = msg.first();
        message = Builder.CreatePointerCast(message, messageTy);
        messageLength = emitter.computeTotalBoundLength(Builder, msg.second());
    }
    else {
        message = llvm::ConstantPointerNull::get(messageTy);
        messageLength = llvm::ConstantInt::get(CG.getInt32Ty(), 0);
    }

    llvm::Value *fileName = CG.getModuleName();
    llvm::Value *lineNum = CG.getSourceLine(pragma->getLocation());
    CRT.raiseAssertionError(SRF, fileName, lineNum, message, messageLength);

    // Switch to the continuation block.
    Builder.SetInsertPoint(passBB);
}

// FIXME: We should have an AttributeEmitter to handle all attribute codegen.
std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitRangeAttrib(RangeAttrib *attrib)
{
    typedef std::pair<llvm::Value*, llvm::Value*> BoundPair;
    BoundsEmitter emitter(*this);
    BoundPair bounds;

    if (ArrayRangeAttrib *arrayRange = dyn_cast<ArrayRangeAttrib>(attrib)) {
        CValue arrValue = emitArrayExpr(arrayRange->getPrefix(), 0, false);
        bounds = emitter.getBounds(Builder, arrValue.second(), 0);
    }
    else {
        // FIXME: This evaluation is wrong.  All types should be elaborated
        // and the range information accessible.  The following is
        // effectively a "re-elaboration" of the prefix type.
        ScalarRangeAttrib *scalarRange = cast<ScalarRangeAttrib>(attrib);
        DiscreteType *scalarTy = scalarRange->getType();
        bounds = emitter.getScalarBounds(Builder, scalarTy);
    }
    return bounds;
}

Type *CodeGenRoutine::resolveType(Type *type)
{
    return const_cast<Type*>(CGT.resolveType(type));
}
