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

#include "CodeGenRoutine.h"
#include "CodeGenTypes.h"
#include "comma/ast/Type.h"
#include "comma/ast/Expr.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

llvm::Value *CodeGenRoutine::computeArrayLength(llvm::Value *bounds)
{
    // Interrogate the layout of the given bounds structure and compute the
    // total length of the array.
    const llvm::SequentialType *seqTy;
    const llvm::StructType *strTy;

    seqTy = cast<llvm::PointerType>(bounds->getType());
    strTy = cast<llvm::StructType>(seqTy->getElementType());

    const llvm::IntegerType *sumTy = CG.getInt64Ty();
    llvm::Value *length = llvm::ConstantInt::get(sumTy, int64_t(0));
    unsigned idx = 0;
    unsigned numElts = strTy->getNumElements();

    while(idx < numElts) {
        llvm::Value *lower = Builder.CreateStructGEP(bounds, idx++);
        lower = Builder.CreateLoad(lower);

        llvm::Value *upper = Builder.CreateStructGEP(bounds, idx++);
        upper = Builder.CreateLoad(upper);

        // Sign extend the bounds if needed.
        //
        // FIXME: Zero extend modular types here when implemented.
        const llvm::IntegerType *boundTy;
        boundTy = cast<llvm::IntegerType>(lower->getType());
        if (boundTy->getBitWidth() < sumTy->getBitWidth()) {
            lower = Builder.CreateSExt(lower, sumTy);
            upper = Builder.CreateSExt(upper, sumTy);
        }

        // The length of this array dimension is upper - lower + 1.
        length = Builder.CreateAdd(length, upper);
        length = Builder.CreateSub(length, lower);
        length = Builder.CreateAdd(length,
                                   llvm::ConstantInt::get(sumTy, int64_t(1)));
    }
    return length;
}

/// \brief Given an array type with statically constrained indices, synthesizes
/// a constant LLVM structure representing the bounds of the array.
llvm::Constant *CodeGenRoutine::synthStaticArrayBounds(ArrayType *arrTy)
{
    const llvm::StructType *boundsTy = CGT.lowerArrayBounds(arrTy);
    std::vector<llvm::Constant*> bounds;

    for (unsigned i = 0; i < arrTy->getRank(); ++i) {
        DiscreteType *idxTy = arrTy->getIndexType(i);
        Range *range = idxTy->getConstraint();

        assert(idxTy->isStaticallyConstrained());
        const llvm::APInt &lower = range->getStaticLowerBound();
        const llvm::APInt &upper = range->getStaticUpperBound();

        const llvm::Type *eltTy = boundsTy->getElementType(i);
        bounds.push_back(llvm::ConstantInt::get(eltTy, lower));
        bounds.push_back(llvm::ConstantInt::get(eltTy, upper));
    }
    return llvm::ConstantStruct::get(boundsTy, bounds);
}

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
                                   llvm::Value *length)
{
    // Scale the length by the size of the component type of the arrays.
    const llvm::SequentialType *seqTy;
    const llvm::Type *compTy;
    llvm::Value *compSize;
    seqTy = cast<llvm::PointerType>(destination->getType());
    seqTy = cast<llvm::ArrayType>(seqTy->getElementType());
    compTy = seqTy->getElementType();
    compSize = llvm::ConstantExpr::getSizeOf(compTy);

    // FIXME: compSize is of type i64.  Need to truncate on 32 bit arch.
    length = Builder.CreateMul(length, compSize);

    // Finally, perform a memcpy from the source to the destination.
    //
    // FIXME: Adjust the intrinsic used to fit the target arch.
    llvm::Constant *align = llvm::ConstantInt::get(CG.getInt32Ty(), 1);
    llvm::Function *memcpy = CG.getMemcpy64();

    seqTy = CG.getInt8PtrTy();
    llvm::Value *src = Builder.CreatePointerCast(source, seqTy);
    llvm::Value *dst = Builder.CreatePointerCast(destination, seqTy);
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
    // stack in the majority of cases.  If the supplied bounds parameter is
    // non-null, load the address of
    llvm::Constant *boundData = synthStaticArrayBounds(expr->getType());
    llvm::GlobalVariable *boundPtr =
        new llvm::GlobalVariable(*CG.getModule(), boundData->getType(), true,
                                 llvm::GlobalValue::InternalLinkage, boundData,
                                 "bounds.data");

    return std::pair<llvm::Value*, llvm::Value*>(dataPtr, boundPtr);
}

std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitArrayExpr(Expr *expr, llvm::Value *dst, bool genTmp)
{
    typedef std::pair<llvm::Value*, llvm::Value*> ArrPair;

    llvm::Value *components;
    llvm::Value *bounds;
    ArrayType *arrTy = cast<ArrayType>(expr->getType());
    ConversionExpr *convert = dyn_cast<ConversionExpr>(expr);

    if (convert) {
        ArrPair result = emitArrayExpr(convert->getOperand(), dst, genTmp);
        emitArrayConversion(convert, result.first, result.second);
        return result;
    }

    if (StringLiteral *lit = dyn_cast<StringLiteral>(expr)) {
        ArrPair pair = emitStringLiteral(lit);
        components = pair.first;
        bounds = Builder.CreateLoad(pair.second);
    }
    else if (DeclRefExpr *ref = dyn_cast<DeclRefExpr>(expr)) {
        ValueDecl *decl = ref->getDeclaration();
        components = lookupDecl(decl);
        bounds = Builder.CreateLoad(lookupBounds(decl));
    }
    else if (AggregateExpr *agg = dyn_cast<AggregateExpr>(expr))
        return emitAggregate(agg, dst, genTmp);
    else {
        assert(false && "Invalid type of array expr!");
        return ArrPair(0, 0);
    }

    // If dst is null and genTmp is true, build a stack allocated object to
    // hold the components of this array.
    if (dst == 0 && genTmp) {
        llvm::BasicBlock *savedBB;
        const llvm::Type *componentTy;
        const llvm::Type *dstTy;
        llvm::Value *dim;
        llvm::Value *length;

        savedBB = Builder.GetInsertBlock();
        Builder.SetInsertPoint(entryBB);

        componentTy = CGT.lowerType(arrTy->getComponentType());
        dstTy = CG.getPointerType(CG.getVLArrayTy(componentTy));

        // FIXME: The length is always computed as an i64, but an alloca
        // instruction requires an i32.  Simply truncate for now.
        length = computeArrayLength(bounds);
        dim = Builder.CreateTrunc(length, CG.getInt32Ty());
        dst = Builder.CreateAlloca(componentTy, dim);
        dst = Builder.CreatePointerCast(dst, dstTy);
        Builder.SetInsertPoint(savedBB);
    }

    // If a destination is available, fill it in with the associated array data.
    if (dst) {
        llvm::Value *length = computeArrayLength(bounds);
        emitArrayCopy(components, dst, length);
        return ArrPair(dst, bounds);
    }
    else
        return ArrPair(components, bounds);
}

llvm::Value *CodeGenRoutine::synthAggregateBounds(AggregateExpr *agg)
{
    llvm::Value *bounds = 0;
    ArrayType *arrTy = cast<ArrayType>(agg->getType());

    if (arrTy->isStaticallyConstrained())
        bounds = synthStaticArrayBounds(arrTy);
    else if (arrTy->isConstrained())
        assert(false && "Cannot codegen dynamicly constrained aggregates yet!");
    else {
        // The index type for the aggregate is unconstrained.
        llvm::APInt bound;
        std::vector<llvm::Constant*> boundValues;
        DiscreteType *idxTy = arrTy->getIndexType(0);
        llvm::LLVMContext &context = CG.getLLVMContext();

        // The lower bound is derived directly from the index type.
        if (Range *range = idxTy->getConstraint()) {
            assert(range->isStatic() && "Cannot codegen dynamic ranges.");
            bound = range->getStaticLowerBound();
            boundValues.push_back(llvm::ConstantInt::get(context, bound));
        }
        else {
            idxTy->getLowerLimit(bound);
            boundValues.push_back(llvm::ConstantInt::get(context, bound));
        }

        // The upper bound is the sum of the lower bound and the length of the
        // aggregate, minus one.
        llvm::APInt length(bound.getBitWidth(), agg->numComponents());
        bound += --length;
        boundValues.push_back(llvm::ConstantInt::get(context, bound));

        // Obtain a constant structure object corresponding to the computed
        // bounds.
        const llvm::StructType *boundsTy = CGT.lowerArrayBounds(arrTy);
        bounds = llvm::ConstantStruct::get(boundsTy, boundValues);
    }
    return bounds;
}


std::pair<llvm::Value*, llvm::Value*>
CodeGenRoutine::emitAggregate(AggregateExpr *expr, llvm::Value *dst,
                              bool genTmp)
{
    typedef std::pair<llvm::Value*, llvm::Value*> ArrPair;
    typedef AggregateExpr::component_iter iterator;

    llvm::Value *bounds = synthAggregateBounds(expr);
    ArrayType *arrTy = cast<ArrayType>(expr->getType());

    std::vector<llvm::Value*> components;

    if (!dst && genTmp) {
        // Compute the length of the array using the bounds.
        llvm::Value *lower = Builder.CreateExtractValue(bounds, 0);
        llvm::Value *upper = Builder.CreateExtractValue(bounds, 1);

        // FIXME: LLVM alloca's are currently performed using i32 (though this
        // might change).  The bounds may be a wider type.  We need to insert
        // checks the array bounds do not overflow using this precision.
        const llvm::IntegerType *boundTy
            = cast<llvm::IntegerType>(lower->getType());

        if (boundTy->getBitWidth() < 32) {
            lower = Builder.CreateSExt(lower, CG.getInt32Ty());
            upper = Builder.CreateSExt(upper, CG.getInt32Ty());
        }
        else if (boundTy->getBitWidth() > 32) {
            lower = Builder.CreateTrunc(lower, CG.getInt32Ty());
            upper = Builder.CreateTrunc(upper, CG.getInt32Ty());
        }

        llvm::Value *size = Builder.CreateSub(upper, lower);
        llvm::Value *one = llvm::ConstantInt::get(size->getType(), 1);
        size = Builder.CreateAdd(size, one);

        const llvm::Type *elemTy = CGT.lowerType(arrTy->getComponentType());
        const llvm::Type *dstTy = CG.getPointerType(CG.getVLArrayTy(elemTy));
        dst = Builder.CreateAlloca(elemTy, size);
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

    return ArrPair(dst, bounds);
}

void CodeGenRoutine::emitArrayConversion(ConversionExpr *conver,
                                         llvm::Value *components,
                                         llvm::Value *bounds)
{
    // FIXME: Implement.
}
