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

CValue CodeGenRoutine::emitDeclRefExpr(DeclRefExpr *expr)
{
    ValueDecl *refDecl = expr->getDeclaration();
    Type *exprType = resolveType(expr->getType());
    llvm::Value *exprValue = SRF->lookup(refDecl, activation::Slot);

    // Fat access types are always represented as a pointer to the underlying
    // data.  Regardless of the actual declaration kind we have the proper
    // representation.
    if (exprType->isFatAccessType())
        return CValue::getFat(exprValue);

    // If we have a renamed object decl which simply renames another declaration
    // (after stripping any inj/prj's) emit the object as an alias for its rhs.
    // Otherwise the renamed declaration is associated with a slot to the
    // (already emitted) value.
    if (RenamedObjectDecl *ROD = dyn_cast<RenamedObjectDecl>(refDecl)) {
        Expr *renamedExpr = ROD->getRenamedExpr()->ignoreInjPrj();
        if (DeclRefExpr *DRE = dyn_cast<DeclRefExpr>(renamedExpr))
            return emitDeclRefExpr(DRE);
        else
            return CValue::get(Builder.CreateLoad(exprValue));
    }

    // For an object declaration just load the value.
    if (isa<ObjectDecl>(refDecl))
        return CValue::get(Builder.CreateLoad(exprValue));

    // If the declaration references a parameter and the mode is either "out" or
    // "in out", load the actual value.
    if (ParamValueDecl *pvDecl = dyn_cast<ParamValueDecl>(refDecl)) {
        PM::ParameterMode paramMode = pvDecl->getParameterMode();
        if (paramMode == PM::MODE_OUT || paramMode == PM::MODE_IN_OUT)
            exprValue = Builder.CreateLoad(exprValue);
        return CValue::get(exprValue);
    }

    // LoopDecl's are always associated directly with their value.
    if (isa<LoopDecl>(refDecl))
        return CValue::get(exprValue);

    assert(false && "Unexpected type of expression!");
    return CValue::get(0);
}

CValue CodeGenRoutine::emitInjExpr(InjExpr *expr)
{
    return emitValue(expr->getOperand());
}

CValue CodeGenRoutine::emitPrjExpr(PrjExpr *expr)
{
    return emitValue(expr->getOperand());
}

CValue CodeGenRoutine::emitNullExpr(NullExpr *expr)
{
    AccessType *access = cast<AccessType>(resolveType(expr));

    if (access->isThinAccessType()) {
        const llvm::PointerType *loweredTy;
        loweredTy = CGT.lowerThinAccessType(access);
        return CValue::get(llvm::ConstantPointerNull::get(loweredTy));
    }
    else {
        const llvm::StructType *loweredTy;
        const llvm::PointerType *dataTy;
        llvm::Value *fatPtr;

        loweredTy = CGT.lowerFatAccessType(access);
        fatPtr = SRF->createTemp(loweredTy);
        dataTy = cast<llvm::PointerType>(loweredTy->getElementType(0));

        Builder.CreateStore(llvm::ConstantPointerNull::get(dataTy),
                            Builder.CreateStructGEP(fatPtr, 0));
        return CValue::getFat(fatPtr);
    }
}

CValue CodeGenRoutine::emitIntegerLiteral(IntegerLiteral *expr)
{
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

    return CValue::get(llvm::ConstantInt::get(CG.getLLVMContext(), val));
}

CValue CodeGenRoutine::emitIndexedArrayRef(IndexedArrayExpr *IAE)
{
    assert(IAE->getNumIndices() == 1 &&
           "Multidimensional arrays are not yet supported!");

    Expr *arrExpr = IAE->getPrefix();
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
            bounds = CRT.vstack(Builder, boundTy->getPointerTo());
            bounds = Builder.CreateLoad(bounds);
            CRT.vstack_pop(Builder);

            // Set the array data to the current top of the stack.
            data = CRT.vstack(Builder, dataTy->getPointerTo());
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
        CValue arrValue = emitArrayExpr(arrExpr, 0, false);
        data = arrValue.first();
        bounds = arrValue.second();
    }

    // Emit and adjust the index by the lower bound of the array.  Adjust to the
    // system pointer width if needed.
    llvm::Value *index = emitValue(idxExpr).first();
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
        component = componentSlot;
    }

    // Return the appropriate CValue for the component type.
    Type *componentTy = resolveType(arrTy->getComponentType());

    if (componentTy->isArrayType()) {
        arrTy = cast<ArrayType>(componentTy);
        return CValue::getArray(component, BE.synthArrayBounds(Builder, arrTy));
    }

    if (componentTy->isFatAccessType())
        return CValue::getFat(component);

    return CValue::get(component);
}

CValue CodeGenRoutine::emitIndexedArrayValue(IndexedArrayExpr *expr)
{
    CValue addr = emitIndexedArrayRef(expr);
    if (addr.isSimple())
        return CValue::get(Builder.CreateLoad(addr.first()));
    else
        return addr;
}

CValue CodeGenRoutine::emitSelectedRef(SelectedExpr *expr)
{
    // Currently, the prefix of a SelectedExpr is always of record type.
    CValue record = emitRecordExpr(expr->getPrefix(), 0, false);
    ComponentDecl *component = cast<ComponentDecl>(expr->getSelectorDecl());

    // Find the index into into the record and GEP the component.
    unsigned index = CGT.getComponentIndex(component);
    llvm::Value *ptr = Builder.CreateStructGEP(record.first(), index);

    Type *componentTy = resolveType(expr);
    if (componentTy->isFatAccessType())
        return CValue::getFat(ptr);

    // Arrays are always constrained inside records.
    if (componentTy->isArrayType()) {
        ArrayType *arrTy = cast<ArrayType>(componentTy);
        BoundsEmitter emitter(*this);
        llvm::Value *bounds = emitter.synthArrayBounds(Builder, arrTy);
        return CValue::getArray(ptr, bounds);
    }

    return CValue::get(ptr);
}

CValue CodeGenRoutine::emitSelectedValue(SelectedExpr *expr)
{
    CValue componentPtr = emitSelectedRef(expr);
    if (componentPtr.isSimple())
        return CValue::get(Builder.CreateLoad(componentPtr.first()));
    else
        return componentPtr;
}

CValue CodeGenRoutine::emitDereferencedValue(DereferenceExpr *expr)
{
    CValue value = emitValue(expr->getPrefix());
    llvm::Value *pointer = value.first();
    emitNullAccessCheck(pointer, expr->getLocation());
    return CValue::get(Builder.CreateLoad(pointer));
}

CValue CodeGenRoutine::emitConversionValue(ConversionExpr *expr)
{
    // The only type of conversions we currently support are those which involve
    // discrete types.
    if (DiscreteType *target = dyn_cast<DiscreteType>(expr->getType())) {
        llvm::Value *value = emitDiscreteConversion(expr->getOperand(), target);
        return CValue::get(value);
    }

    assert(false && "Cannot codegen given conversion yet!");
    return CValue::get(0);
}

CValue CodeGenRoutine::emitDefaultValue(Type *type)
{
    type = resolveType(type);
    const llvm::Type *loweredTy = CGT.lowerType(type);

    // Default value for a fat access value is a null temporary.
    if (type->isFatAccessType()) {
        llvm::Value *slot = SRF->createTemp(loweredTy);
        llvm::Value *fatValue = llvm::ConstantAggregateZero::get(loweredTy);
        Builder.CreateStore(fatValue, slot);
        return CValue::getFat(slot);
    }

    // Null pointers for thin access types.
    if (type->isThinAccessType()) {
        const llvm::PointerType *ptrTy = cast<llvm::PointerType>(loweredTy);
        return CValue::get(llvm::ConstantPointerNull::get(ptrTy));
    }

    // FIXME: Currently all other values are simple integers.
    return CValue::get(llvm::ConstantInt::get(loweredTy, 0));
}

CValue CodeGenRoutine::emitAllocatorValue(AllocatorExpr *expr)
{
    Type *allocatedType = resolveType(expr->getAllocatedType());
    if (allocatedType->isCompositeType())
        return emitCompositeAllocator(expr);

    // Compute the size and alignment of the type to be allocated.
    AccessType *exprTy = expr->getType();
    const llvm::PointerType *resultTy = CGT.lowerThinAccessType(exprTy);
    const llvm::Type *pointeeTy = resultTy->getElementType();

    uint64_t size = CGT.getTypeSize(pointeeTy);
    unsigned align = CGT.getTypeAlignment(pointeeTy);

    // Call into the runtime to allocate the object and cast the result to the
    // needed type.
    llvm::Value *pointer = CRT.comma_alloc(Builder, size, align);
    pointer = Builder.CreatePointerCast(pointer, resultTy);

    // If the allocator is initialized emit the object into the allocated
    // memory.
    if (expr->isInitialized()) {
        Expr *init = expr->getInitializer();
        Builder.CreateStore(emitValue(init).first(), pointer);
    }

    return CValue::get(pointer);
}

void
CodeGenRoutine::emitDiscreteRangeCheck(llvm::Value *sourceVal, Location loc,
                                       Type *sourceTy, DiscreteType *targetTy)
{
    const llvm::IntegerType *loweredSourceTy;
    const llvm::IntegerType *loweredTargetTy;
    loweredSourceTy = cast<llvm::IntegerType>(CGT.lowerType(sourceTy));
    loweredTargetTy = cast<llvm::IntegerType>(CGT.lowerType(targetTy));

    // The "domain of computation" used for performing the range check.
    const llvm::IntegerType *docTy;

    // Determine if the source type is signed.  The type universal_integer is
    // always considered as unsigned.
    bool isSigned;
    if (DiscreteType *discTy = dyn_cast<DiscreteType>(sourceTy))
        isSigned = discTy->isSigned();
    else {
        assert(sourceTy->isUniversalIntegerType());
        isSigned = false;
    }

    // Range checks need to be performed using the larger type (most often the
    // source type).  Find the appropriate type and extend the value if needed.
    if (loweredTargetTy->getBitWidth() > loweredSourceTy->getBitWidth()) {
        docTy = loweredTargetTy;
        if (isSigned)
            sourceVal = Builder.CreateSExt(sourceVal, docTy);
        else
            sourceVal = Builder.CreateZExt(sourceVal, docTy);
    }
    else
        docTy = loweredSourceTy;

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
    if (loweredTargetTy->getBitWidth() < docTy->getBitWidth()) {
        if (targetTy->isSigned()) {
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
    llvm::Value *fileName = CG.getModuleName();
    llvm::Value *lineNum = CG.getSourceLine(loc);
    llvm::GlobalVariable *msg = CG.emitInternString("Range check failed!");
    CRT.raiseConstraintError(SRF, fileName, lineNum, msg);

    // Switch the context to the success block.
    Builder.SetInsertPoint(checkMergeBB);
}

void CodeGenRoutine::emitNullAccessCheck(llvm::Value *pointer, Location loc)
{
    llvm::BasicBlock *passBlock = SRF->makeBasicBlock("null.check.pass");
    llvm::BasicBlock *failBlock = SRF->makeBasicBlock("null.check.fail");

    llvm::Value *pred = Builder.CreateIsNull(pointer);

    Builder.CreateCondBr(pred, failBlock, passBlock);

    Builder.SetInsertPoint(failBlock);
    llvm::Value *fileName = CG.getModuleName();
    llvm::Value *lineNum = CG.getSourceLine(loc);
    llvm::GlobalVariable *msg = CG.emitInternString("Null check failed.");
    CRT.raiseProgramError(SRF, fileName, lineNum, msg);

    // Switch to the pass block.
    Builder.SetInsertPoint(passBlock);
}

llvm::Value *CodeGenRoutine::emitDiscreteConversion(Expr *expr,
                                                    DiscreteType *targetTy)
{
    // Evaluate the source expression.
    Type *exprTy = expr->getType();
    llvm::Value *sourceVal = emitValue(expr).first();
    const llvm::IntegerType *loweredTy = CGT.lowerDiscreteType(targetTy);

    // If the expression and target types are identical, we are done.
    if (exprTy == targetTy)
        return sourceVal;

    unsigned sourceWidth = cast<llvm::IntegerType>(sourceVal->getType())->getBitWidth();
    unsigned targetWidth = loweredTy->getBitWidth();

    if (DiscreteType *sourceTy = dyn_cast<DiscreteType>(exprTy)) {
        // If the target type contains the source type then a range check is
        // unnessary.
        if (targetTy->contains(sourceTy) == DiscreteType::Is_Contained) {
            if (targetWidth == sourceWidth)
                return sourceVal;
            else if (targetWidth > sourceWidth) {
                if (targetTy->isSigned())
                    return Builder.CreateSExt(sourceVal, loweredTy);
                else
                    return Builder.CreateZExt(sourceVal, loweredTy);
            }
        }
    }
    else {
        // The expression must be of type universal_integer.  Always emit a
        // range check in this case.
        assert(exprTy->isUniversalIntegerType() &&
               "Unexpected expression type!");
    }

    emitDiscreteRangeCheck(sourceVal, expr->getLocation(), exprTy, targetTy);

    // Truncate/extend the value if needed to the target size.
    if (targetWidth < sourceWidth)
        sourceVal = Builder.CreateTrunc(sourceVal, loweredTy);
    else if (targetWidth > sourceWidth) {
        if (targetTy->isSigned())
            sourceVal = Builder.CreateSExt(sourceVal, loweredTy);
        else
            sourceVal = Builder.CreateZExt(sourceVal, loweredTy);
    }
    return sourceVal;
}

CValue CodeGenRoutine::emitAttribExpr(AttribExpr *expr)
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

    return CValue::get(result);
}

llvm::Value *CodeGenRoutine::emitScalarBoundAE(ScalarBoundAE *AE)
{
    BoundsEmitter emitter(*this);
    DiscreteType *Ty = AE->getType();
    if (AE->isFirst())
        return emitter.getLowerBound(Builder, Ty);
    else
        return emitter.getUpperBound(Builder, Ty);
}

llvm::Value *CodeGenRoutine::emitArrayBoundAE(ArrayBoundAE *AE)
{
    BoundsEmitter emitter(*this);
    ArrayType *arrTy = AE->getPrefixType();

    if (arrTy->isConstrained()) {
        // For constrained arrays the bound can be generated with reference to
        // the index subtype alone.
        IntegerType *indexTy = AE->getType();
        if (AE->isFirst())
            return emitter.getLowerBound(Builder, indexTy);
        else
            return emitter.getUpperBound(Builder, indexTy);
    }

    // Otherwise emit the prefix as a reference to the array and obtain the
    // bounds.
    CValue arrValue = emitCompositeExpr(AE->getPrefix(), 0, false);
    llvm::Value *bounds = arrValue.second();
    unsigned dimension = AE->getDimension();

    if (AE->isFirst())
        return emitter.getLowerBound(Builder, bounds, dimension);
    else
        return emitter.getUpperBound(Builder, bounds, dimension);
}
