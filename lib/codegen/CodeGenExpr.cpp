//===-- codegen/CodeGenExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

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

llvm::Value *CodeGenRoutine::emitIndexedArrayRef(IndexedArrayExpr *expr)
{
    assert(expr->getNumIndices() == 1 &&
           "Multidimensional arrays are not yet supported!");

    DeclRefExpr *arrRefExpr = expr->getArrayExpr();
    Expr *idxExpr = expr->getIndex(0);
    llvm::Value *idxValue = emitValue(idxExpr);
    llvm::Value *arrValue =
        SRF->lookup(arrRefExpr->getDeclaration(), activation::Slot);

    ArrayType *arrType = cast<ArrayType>(arrRefExpr->getType());

    if (arrType->isConstrained()) {
        // Resolve the index type of the array (not the type of the index
        // expression).
        DiscreteType *indexType = arrType->getIndexType(0);

        // If the index type is an integer type adjust the index expression by
        // the lower bound.  If the bound happens to be a constant zero the
        // IRBuilder will fold the ajustment.
        if (IntegerType *intTy = dyn_cast<IntegerType>(indexType)) {
            llvm::Value *adjust = emitScalarLowerBound(intTy);
            idxValue = Builder.CreateSub(idxValue, adjust);
        }
    }
    else {
        // The array expression is unconstrained.  Lookup the bounds for the
        // array and adjust the index if needed.
        ValueDecl *decl = expr->getArrayExpr()->getDeclaration();
        llvm::Value *boundSlot = SRF->lookup(decl, activation::Bounds);
        assert(boundSlot && "Could not retrieve array bounds!");

        // Grab the lower bound and subtract it from the index.
        llvm::Value *bounds = Builder.CreateConstGEP1_32(boundSlot, 0);
        llvm::Value *lower = Builder.CreateStructGEP(bounds, 0);
        llvm::Value *adjust = Builder.CreateLoad(lower);
        idxValue = Builder.CreateSub(idxValue, adjust);
    }

    // Arrays are always represented as pointers to the aggregate. GEP the
    // component.
    llvm::Value *indices[2];
    indices[0] = llvm::ConstantInt::get(CG.getInt32Ty(), (uint64_t)0);
    indices[1] = idxValue;
    return Builder.CreateGEP(arrValue, indices, indices + 2);
}

llvm::Value *CodeGenRoutine::emitIndexedArrayValue(IndexedArrayExpr *expr)
{
    llvm::Value *addr = emitIndexedArrayRef(expr);
    return Builder.CreateLoad(addr);
}

llvm::Value *CodeGenRoutine::emitConversionValue(ConversionExpr *expr)
{
    // The only type of conversions we currently support are integer
    // conversions.
    if (IntegerType *target = dyn_cast<IntegerType>(expr->getType()))
        return emitCheckedIntegerConversion(expr->getOperand(), target);

    assert(false && "Cannot codegen given conversion yet!");
    return 0;
}

void
CodeGenRoutine::emitScalarRangeCheck(llvm::Value *sourceVal,
                                     IntegerType *sourceTy,
                                     IntegerType *targetTy)
{
    IntegerType *targetRootTy = targetTy->getRootType();
    IntegerType *sourceRootTy = sourceTy->getRootType();
    const llvm::IntegerType *boundTy;

    // Range checks need to be performed using the larger type (most often the
    // source type).  Find the appropriate type and sign extend the value if
    // needed.
    if (targetRootTy->getSize() > sourceRootTy->getSize()) {
        boundTy = CGT.lowerIntegerType(targetTy);
        sourceVal = Builder.CreateSExt(sourceVal, boundTy);
    }
    else
        boundTy = cast<llvm::IntegerType>(sourceVal->getType());

    llvm::APInt lower;
    llvm::APInt upper;

    // If the target subtype is constrained, extract the bounds of the
    // constraint.  Otherwise, use the bounds of the base type.
    //
    // FIXME: Support dynmaic ranges.
    if (Range *range = targetTy->getConstraint()) {
        assert(range->isStatic() && "Dynamic ranges not supported yet!");
        lower = range->getStaticLowerBound();
        upper = range->getStaticUpperBound();
    }
    else {
        targetRootTy->getLowerLimit(lower);
        targetRootTy->getUpperLimit(upper);
    }

    if (lower.getBitWidth() < boundTy->getBitWidth())
        lower.sext(boundTy->getBitWidth());
    if (upper.getBitWidth() < boundTy->getBitWidth())
        upper.sext(boundTy->getBitWidth());

    // Obtain constants for the bounds.
    llvm::Constant *lowBound = llvm::ConstantInt::get(boundTy, lower);
    llvm::Constant *highBound = llvm::ConstantInt::get(boundTy, upper);

    // Build our basic blocks.
    llvm::BasicBlock *checkHighBB = makeBasicBlock("high.check");
    llvm::BasicBlock *checkFailBB = makeBasicBlock("check.fail");
    llvm::BasicBlock *checkMergeBB = makeBasicBlock("check.merge");

    // Check the low bound.
    llvm::Value *lowPass = Builder.CreateICmpSLE(lowBound, sourceVal);
    Builder.CreateCondBr(lowPass, checkHighBB, checkFailBB);

    // Check the high bound.
    Builder.SetInsertPoint(checkHighBB);
    llvm::Value *highPass = Builder.CreateICmpSLE(sourceVal, highBound);
    Builder.CreateCondBr(highPass, checkMergeBB, checkFailBB);

    // Raise an exception if the check failed.
    Builder.SetInsertPoint(checkFailBB);
    llvm::GlobalVariable *msg = CG.emitInternString("Range check failed!");
    CRT.raise(Builder, msg);

    // Switch the context to the success block.
    Builder.SetInsertPoint(checkMergeBB);
}

llvm::Value *
CodeGenRoutine::emitCheckedIntegerConversion(Expr *expr,
                                             IntegerType *targetTy)
{
    IntegerType *sourceTy = cast<IntegerType>(expr->getType());
    unsigned targetWidth = targetTy->getSize();
    unsigned sourceWidth = sourceTy->getSize();

    // Evaluate the source expression.
    llvm::Value *sourceVal = emitValue(expr);

    // If the source and target types are identical, we are done.
    if (sourceTy == targetTy)
        return sourceVal;

    // If the target type contains the source type then a range check is unnessary.
    if (targetTy->contains(sourceTy)) {
        if (targetWidth == sourceWidth)
            return sourceVal;
        if (targetWidth > sourceWidth)
            return Builder.CreateSExt(sourceVal, CGT.lowerType(targetTy));
    }

    emitScalarRangeCheck(sourceVal, sourceTy, targetTy);

    // Truncate/extend the value if needed to the target size.
    if (targetWidth < sourceWidth)
        sourceVal = Builder.CreateTrunc(sourceVal, CGT.lowerType(targetTy));
    if (targetWidth > sourceWidth)
        sourceVal = Builder.CreateSExt(sourceVal, CGT.lowerType(targetTy));
    return sourceVal;
}

llvm::Value *CodeGenRoutine::emitScalarLowerBound(IntegerType *Ty)
{
    const llvm::IntegerType *loweredTy = CGT.lowerIntegerType(Ty);

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

llvm::Value *CodeGenRoutine::emitScalarUpperBound(IntegerType *Ty)
{
    const llvm::IntegerType *loweredTy = CGT.lowerIntegerType(Ty);

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
    IntegerType *Ty = AE->getType();
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
