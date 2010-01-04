//===-- codegen/CodeGenExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "CommaRT.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Expr.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::cast;
using llvm::isa;

llvm::Value *CodeGenRoutine::emitDeclRefExpr(DeclRefExpr *expr)
{
    DeclRefExpr *refExpr = cast<DeclRefExpr>(expr);
    ValueDecl *refDecl = refExpr->getDeclaration();
    llvm::Value *exprValue = SRF->lookup(refDecl, activation::Slot);

    // LoopDecl's are always associated directly with their value.
    if (isa<LoopDecl>(refDecl))
        return exprValue;

    // If the expression is a composite type, just return the associated value.
    if (expr->getType()->isCompositeType())
        return exprValue;

    // If the declaration references a parameter and the mode is either "out" or
    // "in out", load the actual value.
    if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
        PM::ParameterMode paramMode = pvDecl->getParameterMode();

        if (paramMode == PM::MODE_OUT || paramMode == PM::MODE_IN_OUT)
            exprValue = Builder.CreateLoad(exprValue);
        return exprValue;
    }

    // Otherwise, the given expression must reference an object declaration
    // (which are always alloca'd).  Load the value.
    return Builder.CreateLoad(exprValue);
}

llvm::Value *CodeGenRoutine::emitInjExpr(InjExpr *expr)
{
    return emitValue(expr->getOperand());
}

llvm::Value *CodeGenRoutine::emitPrjExpr(PrjExpr *expr)
{
    return emitValue(expr->getOperand());
}

llvm::Value *CodeGenRoutine::emitIntegerLiteral(IntegerLiteral *expr)
{
    assert(expr->hasType() && "Unresolved literal type!");

    const llvm::IntegerType *ty =
        cast<llvm::IntegerType>(CGT.lowerType(expr->getType()));
    llvm::APInt val(expr->getValue());

    // All comma integer literals are represented as signed APInt's.  Sign
    // extend the value if needed to fit in the representation type.
    unsigned valWidth = val.getBitWidth();
    unsigned tyWidth = ty->getBitWidth();
    assert(valWidth <= tyWidth && "Value/Type width mismatch!");

    if (valWidth < tyWidth)
        val.sext(tyWidth);

    return llvm::ConstantInt::get(CG.getLLVMContext(), val);
}

llvm::Value *CodeGenRoutine::emitIndexedArrayRef(IndexedArrayExpr *IAE)
{
    assert(IAE->getNumIndices() == 1 &&
           "Multidimensional arrays are not yet supported!");

    Expr *arrExpr = IAE->getArrayExpr();
    Expr *idxExpr = IAE->getIndex(0);
    ArrayType *arrTy = cast<ArrayType>(arrExpr->getType());

    // Values for the array components and bounds.
    llvm::Value *data;
    llvm::Value *bounds;

    // Lowered types for the array components and bounds.
    const llvm::ArrayType *dataTy = CGT.lowerArrayType(arrTy);
    const llvm::Type *boundTy = CGT.lowerArrayBounds(arrTy);

    // Set to true if the vstack needs popping after the indexed component is
    // loaded (this happens when the prefix is a function call returning an
    // unconstrained array value).
    bool popVstack = false;

    BoundsEmitter BE(*this);

    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(arrExpr)) {
        if (!arrTy->isConstrained()) {
            // Perform a simple call.  This leaves the vstack alone, so the
            // bounds and data are still available.
            emitSimpleCall(call);

            // Load the bounds from the top of the vstack.
            bounds = CRT.vstack(Builder, CG.getPointerType(boundTy));
            bounds = Builder.CreateLoad(bounds);
            CRT.vstack_pop(Builder);

            // Set the array data to the current top of the stack.
            data = CRT.vstack(Builder, CG.getPointerType(dataTy));
            popVstack = true;
        }
        else {
            // FIXME: Otherwise, only statically constraind array types are
            // supported.
            assert(arrTy->isStaticallyConstrained() &&
                   "Cannot codegen dynamicly constrained arrays yet!");

            // Synthesize the bounds and generate a temporary to hold the sret
            // result.
            bounds = BE.synthStaticArrayBounds(Builder, arrTy);
            data = SRF->createTemp(dataTy);
            emitCompositeCall(call, data);
        }
    }
    else {
        std::pair<llvm::Value*, llvm::Value*> arrPair;
        arrPair = emitArrayExpr(arrExpr, 0, false);
        data = arrPair.first;
        bounds = arrPair.second;
    }

    // Emit and adjust the index by the lower bound of the array.  Adjust to the
    // system pointer width if needed.
    llvm::Value *index = emitValue(idxExpr);
    llvm::Value *lowerBound = BE.getLowerBound(Builder, bounds, 0);
    index = Builder.CreateSub(index, lowerBound);
    if (index->getType() != CG.getIntPtrTy())
        index = Builder.CreateIntCast(index, CG.getIntPtrTy(), false);

    // Arrays are always represented as pointers to the aggregate. GEP the
    // component.
    llvm::Value *component;
    llvm::Value *indices[2];
    indices[0] = llvm::ConstantInt::get(CG.getInt32Ty(), (uint64_t)0);
    indices[1] = index;
    component = Builder.CreateInBoundsGEP(data, indices, indices + 2);

    // If popVstack is true, we must allocate a temporary to hold the component
    // before we pop the vstack.  Since we are generating a reference to the
    // indexed component, return the pointer to the slot.  Otherwise, just
    // return the result of the GEP.
    if (popVstack) {
        llvm::Value *componentSlot = SRF->createTemp(dataTy->getElementType());
        Builder.CreateStore(Builder.CreateLoad(component), componentSlot);
        CRT.vstack_pop(Builder);
        return componentSlot;
    }
    else
        return component;
}

llvm::Value *CodeGenRoutine::emitIndexedArrayValue(IndexedArrayExpr *expr)
{
    llvm::Value *addr = emitIndexedArrayRef(expr);
    return Builder.CreateLoad(addr);
}

llvm::Value *CodeGenRoutine::emitSelectedRef(SelectedExpr *expr)
{
    // Currently, the prefix of a SelectedExpr is always of record type.
    llvm::Value *record = emitRecordExpr(expr->getPrefix(), 0, false);
    ComponentDecl *component = cast<ComponentDecl>(expr->getSelector());

    // Find the index into into the record and GEP the component.
    unsigned index = CGT.getComponentIndex(component);
    return Builder.CreateStructGEP(record, index);
}

llvm::Value *CodeGenRoutine::emitSelectedValue(SelectedExpr *expr)
{
    llvm::Value *componentPtr = emitSelectedRef(expr);
    if (expr->getType()->isCompositeType())
        return componentPtr;
    else
        return Builder.CreateLoad(componentPtr);
}

llvm::Value *CodeGenRoutine::emitConversionValue(ConversionExpr *expr)
{
    // The only type of conversions we currently support are those which involve
    // discrete types.
    if (DiscreteType *target = dyn_cast<DiscreteType>(expr->getType()))
        return emitDiscreteConversion(expr->getOperand(), target);

    assert(false && "Cannot codegen given conversion yet!");
    return 0;
}

void
CodeGenRoutine::emitDiscreteRangeCheck(llvm::Value *sourceVal,
                                       DiscreteType *sourceTy,
                                       DiscreteType *targetTy)
{
    DiscreteType *targetRootTy = targetTy->getRootType();
    DiscreteType *sourceRootTy = sourceTy->getRootType();

    // The "domain of computation" used for performing the range check.
    const llvm::IntegerType *docTy;

    // Range checks need to be performed using the larger type (most often the
    // source type).  Find the appropriate type and extend the value if needed.
    if (targetRootTy->getSize() > sourceRootTy->getSize()) {
        docTy = CGT.lowerDiscreteType(targetTy);
        if (sourceTy->isSigned())
            sourceVal = Builder.CreateSExt(sourceVal, docTy);
        else
            sourceVal = Builder.CreateZExt(sourceVal, docTy);
    }
    else
        docTy = cast<llvm::IntegerType>(sourceVal->getType());

    llvm::Value *lower = 0;
    llvm::Value *upper = 0;

    if (llvm::Value *bounds = SRF->lookup(targetTy, activation::Bounds)) {
        lower = BoundsEmitter::getLowerBound(Builder, bounds, 0);
        upper = BoundsEmitter::getUpperBound(Builder, bounds, 0);
    }
    else {
        BoundsEmitter emitter(*this);
        BoundsEmitter::LUPair bounds =
            emitter.getScalarBounds(Builder, targetTy);
        lower = bounds.first;
        upper = bounds.second;
    }

    // Extend the bounds if needed.
    if (targetTy->getSize() < docTy->getBitWidth()) {
        if (sourceTy->isSigned()) {
            lower = Builder.CreateSExt(lower, docTy);
            upper = Builder.CreateSExt(upper, docTy);
        }
        else {
            lower = Builder.CreateZExt(lower, docTy);
            upper = Builder.CreateZExt(upper, docTy);
        }
    }

    // Build our basic blocks.
    llvm::BasicBlock *checkHighBB = SRF->makeBasicBlock("high.check");
    llvm::BasicBlock *checkFailBB = SRF->makeBasicBlock("check.fail");
    llvm::BasicBlock *checkMergeBB = SRF->makeBasicBlock("check.merge");

    // Check the low bound.
    llvm::Value *lowPass;
    if (targetTy->isSigned())
        lowPass = Builder.CreateICmpSLE(lower, sourceVal);
    else
        lowPass = Builder.CreateICmpULE(lower, sourceVal);
    Builder.CreateCondBr(lowPass, checkHighBB, checkFailBB);

    // Check the high bound.
    Builder.SetInsertPoint(checkHighBB);
    llvm::Value *highPass;
    if (targetTy->isSigned())
        highPass = Builder.CreateICmpSLE(sourceVal, upper);
    else
        highPass = Builder.CreateICmpULE(sourceVal, upper);
    Builder.CreateCondBr(highPass, checkMergeBB, checkFailBB);

    // Raise a CONSTRAINT_ERROR exception if the check failed.
    Builder.SetInsertPoint(checkFailBB);
    llvm::GlobalVariable *msg = CG.emitInternString("Range check failed!");
    CRT.raiseConstraintError(SRF, msg);

    // Switch the context to the success block.
    Builder.SetInsertPoint(checkMergeBB);
}

llvm::Value *CodeGenRoutine::emitDiscreteConversion(Expr *expr,
                                                    DiscreteType *targetTy)
{
    DiscreteType *sourceTy = cast<DiscreteType>(expr->getType());
    unsigned targetWidth = targetTy->getSize();
    unsigned sourceWidth = sourceTy->getSize();

    // Evaluate the source expression.
    llvm::Value *sourceVal = emitValue(expr);

    // If the source and target types are identical, we are done.
    if (sourceTy == targetTy)
        return sourceVal;

    // If the target type contains the source type then a range check is unnessary.
    if (targetTy->contains(sourceTy) == DiscreteType::Is_Contained) {
        if (targetWidth == sourceWidth)
            return sourceVal;
        else if (targetWidth > sourceWidth) {
            if (targetTy->isSigned())
                return Builder.CreateSExt(sourceVal, CGT.lowerType(targetTy));
            else
                return Builder.CreateZExt(sourceVal, CGT.lowerType(targetTy));
        }
    }

    emitDiscreteRangeCheck(sourceVal, sourceTy, targetTy);

    // Truncate/extend the value if needed to the target size.
    if (targetWidth < sourceWidth)
        sourceVal = Builder.CreateTrunc(sourceVal, CGT.lowerType(targetTy));
    else if (targetWidth > sourceWidth) {
        if (targetTy->isSigned())
            sourceVal = Builder.CreateSExt(sourceVal, CGT.lowerType(targetTy));
        else
            sourceVal = Builder.CreateZExt(sourceVal, CGT.lowerType(targetTy));
    }
    return sourceVal;
}

llvm::Value *CodeGenRoutine::emitScalarLowerBound(DiscreteType *Ty)
{
    const llvm::IntegerType *loweredTy = CGT.lowerDiscreteType(Ty);

    // If unconstrained, emit the lower limit of the base type.
    if (!Ty->isConstrained()) {
        llvm::APInt bound;
        Ty->getLowerLimit(bound);
        return CG.getConstantInt(loweredTy, bound);
    }

    Range *range = Ty->getConstraint();

    // Emit a constant if the range has a static lower bound.
    if (range->hasStaticLowerBound()) {
        llvm::APInt bound(range->getStaticLowerBound());
        return CG.getConstantInt(loweredTy, bound);
    }

    // Otherwise, we have a dynamic lower bound.
    return emitValue(range->getLowerBound());
}

llvm::Value *CodeGenRoutine::emitScalarUpperBound(DiscreteType *Ty)
{
    const llvm::IntegerType *loweredTy = CGT.lowerDiscreteType(Ty);

    // If unconstrained, emit the lower limit of the base type.
    if (!Ty->isConstrained()) {
        llvm::APInt bound;
        Ty->getUpperLimit(bound);
        return CG.getConstantInt(loweredTy, bound);
    }

    Range *range = Ty->getConstraint();

    // Emit a constant if the range has a static lower bound.
    if (range->hasStaticUpperBound()) {
        llvm::APInt bound(range->getStaticUpperBound());
        return CG.getConstantInt(loweredTy, bound);
    }

    // Otherwise, we have a dynamic upper bound.
    return emitValue(range->getUpperBound());
}

llvm::Value *CodeGenRoutine::emitAttribExpr(AttribExpr *expr)
{
    llvm::Value *result;

    if (ScalarBoundAE *scalarAE = dyn_cast<ScalarBoundAE>(expr))
        result = emitScalarBoundAE(scalarAE);
    else if (ArrayBoundAE *arrayAE = dyn_cast<ArrayBoundAE>(expr))
        result = emitArrayBoundAE(arrayAE);
    else {
        assert(false && "Cannot codegen attribute yet!");
        result = 0;
    }

    return result;
}

llvm::Value *CodeGenRoutine::emitScalarBoundAE(ScalarBoundAE *AE)
{
    DiscreteType *Ty = AE->getType();
    if (AE->isFirst())
        return emitScalarLowerBound(Ty);
    else
        return emitScalarUpperBound(Ty);
}

llvm::Value *CodeGenRoutine::emitArrayBoundAE(ArrayBoundAE *AE)
{
    ArrayType *arrTy = AE->getPrefixType();

    if (arrTy->isConstrained()) {
        // For constrained arrays the bound can be generated with reference to
        // the index subtype alone.
        IntegerType *indexTy = AE->getType();
        if (AE->isFirst())
            return emitScalarLowerBound(indexTy);
        else
            return emitScalarUpperBound(indexTy);
    }

    // FIXME:  Only a DeclRefExpr prefix is supported for unconstrained arrays
    // at the moment.
    DeclRefExpr *ref = dyn_cast<DeclRefExpr>(AE->getPrefix());

    if (!ref) {
        assert(false && "Unconstrained array attribute not supported yet!");
        return 0;
    }

    llvm::Value *bounds = SRF->lookup(ref->getDeclaration(),
                                      activation::Bounds);
    unsigned offset = AE->getDimension() * 2;

    // The bounds structure is organized as a set of low/high pairs.  Offset
    // points to the low entry -- adjust if needed.
    if (AE->isLast())
        ++offset;

    // GEP and lod the required bound.
    llvm::Value *bound = Builder.CreateStructGEP(bounds, offset);
    return Builder.CreateLoad(bound);
}
