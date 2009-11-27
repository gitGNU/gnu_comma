//===-- codegen/CodeGenAgg.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file defines the subroutines which handle code generation of
/// aggregate values.
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "comma/ast/Type.h"
#include "comma/ast/Expr.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void CodeGenRoutine::emitArrayCopy(llvm::Value *source,
                                   llvm::Value *destination,
                                   ArrayType *Ty)
{
    assert(Ty->isStaticallyConstrained() && "Cannot copy unconstrained arrays!");

    // Implement array copies via memcpy.
    llvm::Value *src;
    llvm::Value *dst;
    llvm::Value *len;
    llvm::Constant *align;
    llvm::Function *memcpy;
    const llvm::PointerType *ptrTy;
    const llvm::ArrayType *arrTy;

    src = Builder.CreatePointerCast(source, CG.getInt8PtrTy());
    dst = Builder.CreatePointerCast(destination, CG.getInt8PtrTy());
    ptrTy = cast<llvm::PointerType>(source->getType());
    arrTy = cast<llvm::ArrayType>(ptrTy->getElementType());
    len = llvm::ConstantExpr::getSizeOf(arrTy);

    // Zero extend the length if not an i64.
    if (len->getType() != CG.getInt64Ty())
        len = Builder.CreateZExt(len, CG.getInt64Ty());

    align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    memcpy = CG.getMemcpy64();

    Builder.CreateCall4(memcpy, dst, src, len, align);
}

void CodeGenRoutine::emitArrayCopy(llvm::Value *source,
                                   llvm::Value *destination,
                                   llvm::Value *length,
                                   const llvm::Type *componentTy)
{
    // Scale the length by the size of the component type of the arrays.  The
    // current convention is that array lengths are represented as i32's.
    // Truncate the component size (an i64).
    llvm::Value *compSize;
    compSize = llvm::ConstantExpr::getSizeOf(componentTy);
    compSize = Builder.CreateTrunc(compSize, CG.getInt32Ty());
    length = Builder.CreateMul(length, compSize);

    // Finally, perform a memcpy from the source to the destination.
    //
    // FIXME: Adjust the intrinsic used to fit the target arch.
    llvm::Constant *align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    llvm::Function *memcpy = CG.getMemcpy32();

    const llvm::Type *i8PtrTy = CG.getInt8PtrTy();
    llvm::Value *src = Builder.CreatePointerCast(source, i8PtrTy);
    llvm::Value *dst = Builder.CreatePointerCast(destination, i8PtrTy);
    Builder.CreateCall4(memcpy, dst, src, length, align);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitStringLiteral(StringLiteral *expr)
{
    assert(expr->hasType() && "Unresolved string literal type!");

    // Build a constant array representing the literal.
    const llvm::ArrayType *arrTy = CGT.lowerArrayType(expr->getType());
    const llvm::Type *elemTy = arrTy->getElementType();
    llvm::StringRef string = expr->getString();

    std::vector<llvm::Constant *> elements;
    const EnumerationDecl *component = expr->getComponentType();
    for (llvm::StringRef::iterator I = string.begin(); I != string.end(); ++I) {
        unsigned encoding = component->getEncoding(*I);
        elements.push_back(llvm::ConstantInt::get(elemTy, encoding));
    }

    // Build a constant global array to represent this literal.
    llvm::Constant *arrData = CG.getConstantArray(elemTy, elements);
    llvm::GlobalVariable *dataPtr =
        new llvm::GlobalVariable(*CG.getModule(), arrData->getType(), true,
                                 llvm::GlobalValue::InternalLinkage, arrData,
                                 "string.data");

    // Likewise, build a constant global to represent the bounds.  Emitting the
    // bounds into memory should be slightly faster then building them on the
    // stack in the majority of cases.
    BoundsEmitter emitter(*this);
    llvm::Constant *boundData =
        emitter.synthStaticArrayBounds(Builder, expr->getType());
    llvm::GlobalVariable *boundPtr =
        new llvm::GlobalVariable(*CG.getModule(), boundData->getType(), true,
                                 llvm::GlobalValue::InternalLinkage,
                                 boundData, "bounds.data");
    return std::pair<llvm::Value*, llvm::Value*>(dataPtr, boundPtr);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    typedef std::pair<llvm::Value*, llvm::Value*> ArrPair;

    llvm::Value *components;
    llvm::Value *bounds;
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    BoundsEmitter emitter(*this);

    if (ConversionExpr *convert = dyn_cast<ConversionExpr>(expr))
        return emitArrayConversion(convert, dst, genTmp);

    if (AggregateExpr *agg = dyn_cast<AggregateExpr>(expr))
        return emitAggregate(agg, dst, genTmp);

    if (StringLiteral *lit = dyn_cast<StringLiteral>(expr)) {
        ArrPair pair = emitStringLiteral(lit);
        components = pair.first;
        bounds = pair.second;
    }
    else if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        components = SRF->lookup(decl, activation::Slot);

        /// FIXME: Bounds lookup will fail when the decl is a ParamValueDecl of
        /// a statically constrained type.  A better API will replace this hack.
        if (!(bounds = SRF->lookup(decl, activation::Bounds)))
            bounds = emitter.synthStaticArrayBounds(Builder, arrTy);
    }
    else {
        assert(false && "Invalid type of array expr!");
        return ArrPair(0, 0);
    }

    // If dst is null build a stack allocated object to hold the components of
    // this array.
    llvm::Value *length = 0;
    const llvm::Type *componentTy = 0;
    if (dst == 0 && genTmp) {
        const llvm::Type *dstTy;
        componentTy = CGT.lowerType(arrTy->getComponentType());
        dstTy = CG.getPointerType(CG.getVLArrayTy(componentTy));
        SRF->stacksave();

        if (isa<llvm::PointerType>(bounds->getType()))
            bounds = Builder.CreateLoad(bounds);

        length = emitter.computeTotalBoundLength(Builder, bounds);
        dst = Builder.CreateAlloca(componentTy, length);
        dst = Builder.CreatePointerCast(dst, dstTy);
    }

    // If a destination is available, fill it in with the associated array data.
    if (isa<llvm::PointerType>(bounds->getType()))
        bounds = Builder.CreateLoad(bounds);
    if (dst) {
        if (length == 0)
            length = emitter.computeTotalBoundLength(Builder, bounds);
        if (componentTy == 0)
            componentTy = CGT.lowerType(arrTy->getComponentType());
        emitArrayCopy(components, dst, length, componentTy);
        return ArrPair(dst, bounds);
    }
    else
        return ArrPair(components, bounds);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitAggregate(AggregateExpr *expr, llvm::Value *dst,
                              bool genTmp)
{
    typedef std::pair<llvm::Value*, llvm::Value*> ArrPair;
    typedef AggregateExpr::component_iter iterator;

    BoundsEmitter emitter(*this);
    llvm::Value *bounds = emitter.synthAggregateBounds(Builder, expr);
    ArrayType *arrTy = cast<ArrayType>(expr->getType());

    std::vector<llvm::Value*> components;

    if (!dst && genTmp) {
        llvm::Value *length = emitter.computeBoundLength(Builder, bounds, 0);
        const llvm::Type *elemTy = CGT.lowerType(arrTy->getComponentType());
        const llvm::Type *dstTy = CG.getPointerType(CG.getVLArrayTy(elemTy));
        dst = Builder.CreateAlloca(elemTy, length);
        dst = Builder.CreatePointerCast(dst, dstTy);
    }

    iterator I = expr->begin_components();
    iterator E = expr->end_components();
    for ( ; I != E; ++I)
        components.push_back(emitValue(*I));

    for (unsigned i = 0; i < components.size(); ++i) {
        llvm::Value *idx = Builder.CreateConstGEP2_64(dst, 0, i);
        Builder.CreateStore(components[i], idx);
    }

    // If this aggregate does not specify an others component we are done.
    //
    // FIXME: If an undefined others component is specified we should be
    // initializing the components with their default value (if any).
    Expr *othersExpr = expr->getOthersExpr();
    if (!othersExpr)
        return ArrPair(dst, bounds);

    // If there were no positional components emitted, emit a single "others"
    // expression.  This simplifies emission of the remaining elements since we
    // can "count from zero" and compare against the upper bound without
    // worrying about overflow.
    unsigned numComponents = components.size();
    if (numComponents == 0) {
        llvm::Value *component = emitValue(othersExpr);
        llvm::Value *idx = Builder.CreateConstGEP2_64(dst, 0, 0);
        Builder.CreateStore(component, idx);
        numComponents = 1;
    }

    // Synthesize a loop to populate the remaining components.  Note that a
    // memset is not possible here since we must re-evaluate the associated
    // expression for each component.
    llvm::BasicBlock *checkBB = makeBasicBlock("others.check");
    llvm::BasicBlock *bodyBB = makeBasicBlock("others.body");
    llvm::BasicBlock *mergeBB = makeBasicBlock("others.merge");

    // Compute the number of "other" components minus one that we need to emit.
    llvm::Value *lower = BoundsEmitter::getLowerBound(Builder, bounds, 0);
    llvm::Value *upper = BoundsEmitter::getUpperBound(Builder, bounds, 0);
    llvm::Value *max = Builder.CreateSub(upper, lower);

    // Obtain a stack location for the index variable used to perform the
    // iteration and initialize to the number of components we have already
    // emitted minus one (which is always valid since we guaranteed above that
    // at least one component has been generated).
    const llvm::Type *idxTy = upper->getType();
    llvm::Value *idxSlot = SRF->createTemp(idxTy);
    llvm::Value *idx = llvm::ConstantInt::get(idxTy, numComponents - 1);
    Builder.CreateStore(idx, idxSlot);

    // Branch to the check BB and test if the index is equal to max.  If it is
    // we are done.
    Builder.CreateBr(checkBB);
    Builder.SetInsertPoint(checkBB);
    idx = Builder.CreateLoad(idxSlot);
    Builder.CreateCondBr(Builder.CreateICmpEQ(idx, max), mergeBB, bodyBB);

    // Move to the body block.  Increment our index and store it for use in the
    // next iteration.
    Builder.SetInsertPoint(bodyBB);
    idx = Builder.CreateAdd(idx, llvm::ConstantInt::get(idxTy, 1));
    Builder.CreateStore(idx, idxSlot);

    // Emit the expression associated with the others clause and store it in the
    // destination array.  Branch back to the test.
    //
    // FIXME: This code only works with non-composite values.
    llvm::Value *indices[2];
    indices[0] = llvm::ConstantInt::get(idxTy, 0);
    indices[1] = idx;
    llvm::Value *component = emitValue(othersExpr);
    llvm::Value *ptr = Builder.CreateInBoundsGEP(dst, indices, indices + 2);
    Builder.CreateStore(component, ptr);
    Builder.CreateBr(checkBB);

    // Set the insert point to the merge BB and return.
    Builder.SetInsertPoint(mergeBB);
    return ArrPair(dst, bounds);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitArrayConversion(ConversionExpr *convert, llvm::Value *dst,
                                    bool genTmp)
{
    // FIXME: Implement.
    return emitArrayExpr(convert->getOperand(), dst, genTmp);
}

void CodeGenRoutine::emitArrayObjectDecl(ObjectDecl *objDecl)
{
    BoundsEmitter emitter(*this);
    ArrayType *arrTy = cast<ArrayType>(objDecl->getType());
    const llvm::Type *boundTy = CGT.lowerArrayBounds(arrTy);
    const llvm::Type *loweredTy = CGT.lowerArrayType(arrTy);
    llvm::Value *bounds = 0;
    llvm::Value *slot = 0;

    if (!objDecl->hasInitializer()) {
        bounds = SRF->createEntry(objDecl, activation::Bounds, boundTy);
        emitter.synthStaticArrayBounds(Builder, arrTy, bounds);

        // FIXME: Support dynamicly sized arrays and default initialization.
        assert(arrTy->isStaticallyConstrained() &&
               "Cannot codegen non-static arrays without initializer!");
        SRF->createEntry(objDecl, activation::Slot, loweredTy);
        return;
    }

    if (arrTy->isStaticallyConstrained())
        slot = SRF->createEntry(objDecl, activation::Slot, loweredTy);

    Expr *init = objDecl->getInitializer();
    if (FunctionCallExpr *call = dyn_cast<FunctionCallExpr>(init)) {
        if (arrTy->isStaticallyConstrained()) {
            // Perform the function call and add the destination to the argument
            // set.
            emitCompositeCall(call, slot);

            // Synthesize bounds for this declaration.
            bounds = SRF->createEntry(objDecl, activation::Bounds, boundTy);
            emitter.synthStaticArrayBounds(Builder, arrTy, bounds);
        }
        else {
            // FIXME: Checks are needed when the initializer is unconstrained
            // but the declaration is.  However, this currently cannot happen,
            // hence the assert.
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
            bounds = SRF->createEntry(objDecl, activation::Bounds, boundTy);
        Builder.CreateStore(result.second, bounds);
    }
}
