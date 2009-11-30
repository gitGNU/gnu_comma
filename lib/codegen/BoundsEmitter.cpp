//===-- codegen/BoundsEmitter.cpp ----------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "BoundsEmitter.h"
#include "comma/ast/Expr.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// Synthesizes a bounds structure of the given type with the given lower and
/// upper bounds.
llvm::Value *synthBounds(llvm::IRBuilder<> &Builder,
                         const llvm::StructType *boundTy,
                         llvm::Value *lower, llvm::Value *upper)
{
    // If both the bounds are constants, build a constant structure.
    if (isa<llvm::Constant>(lower) && isa<llvm::Constant>(upper)) {
        std::vector<llvm::Constant*> elts;
        elts.push_back(cast<llvm::Constant>(lower));
        elts.push_back(cast<llvm::Constant>(upper));
        return llvm::ConstantStruct::get(boundTy, elts);
    }

    // Otherwise, populate an an undef object with the bounds.
    llvm::Value *bounds = llvm::UndefValue::get(boundTy);
    bounds = Builder.CreateInsertValue(bounds, lower, 0);
    bounds = Builder.CreateInsertValue(bounds, upper, 1);
    return bounds;
}

} // end anonymous namespace.

const llvm::StructType *BoundsEmitter::getType(const ArrayType *arrTy)
{
    return CGT.lowerArrayBounds(arrTy);
}

llvm::Value *BoundsEmitter::synthScalarBounds(llvm::IRBuilder<> &Builder,
                                              const DiscreteType *type)
{
    LUPair LU = getScalarBounds(Builder, type);
    llvm::Value *lower = LU.first;
    llvm::Value *upper = LU.second;
    const llvm::StructType *boundTy = CGT.lowerScalarBounds(type);
    return synthBounds(Builder, boundTy, lower, upper);
}

BoundsEmitter::LUPair
BoundsEmitter::getScalarBounds(llvm::IRBuilder<> &Builder,
                               const DiscreteType *type)
{
    LUPair LU;

    if (type->isConstrained()) {
        const Range *range = type->getConstraint();
        LU = getRange(Builder, range);
    }
    else {
        const llvm::Type *loweredTy = CGT.lowerType(type);
        llvm::APInt bound;

        type->getLowerLimit(bound);
        LU.first = llvm::ConstantInt::get(loweredTy, bound);

        type->getUpperLimit(bound);
        LU.second = llvm::ConstantInt::get(loweredTy, bound);
    }

    return LU;
}

llvm::Value *BoundsEmitter::synthRange(llvm::IRBuilder<> &Builder,
                                       const Range *range)
{
    LUPair LU = getRange(Builder, range);
    llvm::Value *lower = LU.first;
    llvm::Value *upper = LU.second;
    const llvm::StructType *boundTy = CGT.lowerRange(range);
    return synthBounds(Builder, boundTy, lower, upper);
}

BoundsEmitter::LUPair BoundsEmitter::getRange(llvm::IRBuilder<> &Builder,
                                              const Range *range)
{
    llvm::Value *lower;
    llvm::Value *upper;
    const llvm::Type *elemTy = CGT.lowerType(range->getType());

    if (range->hasStaticLowerBound()) {
        const llvm::APInt &bound = range->getStaticLowerBound();
        lower = llvm::ConstantInt::get(elemTy, bound);
    }
    else {
        Expr *expr = const_cast<Expr*>(range->getLowerBound());
        lower = CGR.emitValue(expr);
    }

    if (range->hasStaticUpperBound()) {
        const llvm::APInt &bound = range->getStaticUpperBound();
        upper = llvm::ConstantInt::get(elemTy, bound);
    }
    else {
        Expr *expr = const_cast<Expr*>(range->getUpperBound());
        upper = CGR.emitValue(expr);
    }

    return LUPair(lower, upper);
}

llvm::Value *BoundsEmitter::computeBoundLength(llvm::IRBuilder<> &Builder,
                                               llvm::Value *bounds,
                                               unsigned index)
{
    llvm::Value *lower = getLowerBound(Builder, bounds, index);
    llvm::Value *upper = getUpperBound(Builder, bounds, index);

    // FIXME: We always return the length of an array as an i32.  The main
    // motivation for this choice is that LLVM alloca's are currently restricted
    // to this size (though this might change).  Since the bounds may be of a
    // wider type, we need to generate checks that the following calculations do
    // not overflow.
    const llvm::IntegerType *boundTy;
    const llvm::IntegerType *i32Ty;

    boundTy = cast<llvm::IntegerType>(lower->getType());
    i32Ty = CG.getInt32Ty();

    if (boundTy->getBitWidth() < 32) {
        lower = Builder.CreateSExt(lower, i32Ty);
        upper = Builder.CreateSExt(upper, i32Ty);
    }
    else if (boundTy->getBitWidth() > 32) {
        lower = Builder.CreateTrunc(lower, i32Ty);
        upper = Builder.CreateTrunc(upper, i32Ty);
    }

    llvm::Value *size = Builder.CreateSub(upper, lower);
    llvm::Value *one = llvm::ConstantInt::get(size->getType(), 1);
    return Builder.CreateAdd(size, one);
}

llvm::Value *BoundsEmitter::computeTotalBoundLength(llvm::IRBuilder<> &Builder,
                                                    llvm::Value *bounds)
{
    const llvm::StructType *strTy;
    const llvm::IntegerType *sumTy;
    llvm::Value *length;
    unsigned numElts;

    strTy = cast<llvm::StructType>(bounds->getType());
    sumTy = CG.getInt32Ty();

    length = llvm::ConstantInt::get(sumTy, int64_t(0));
    numElts = strTy->getNumElements() / 2;

    for (unsigned idx = 0; idx < numElts; ++idx) {
        llvm::Value *partial = computeBoundLength(Builder, bounds, idx);
        length = Builder.CreateAdd(length, partial);
    }
    return length;
}

llvm::Value *BoundsEmitter::computeIsNull(llvm::IRBuilder<> &Builder,
                                          llvm::Value *bounds, unsigned index)
{
    llvm::Value *lower = getLowerBound(Builder, bounds, index);
    llvm::Value *upper = getUpperBound(Builder, bounds, index);
    return Builder.CreateICmpSLT(upper, lower);
}

llvm::Constant *
BoundsEmitter::synthStaticArrayBounds(llvm::IRBuilder<> &Builder,
                                      ArrayType *arrTy, llvm::Value *dst)
{
    const llvm::StructType *boundsTy = getType(arrTy);
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
    llvm::Constant *data = llvm::ConstantStruct::get(boundsTy, bounds);

    if (dst)
        Builder.CreateStore(data, dst);
    return data;
}

llvm::Value *BoundsEmitter::synthAggregateBounds(llvm::IRBuilder<> &Builder,
                                                 PositionalAggExpr *agg,
                                                 llvm::Value *dst)
{
    llvm::Value *bounds = 0;
    ArrayType *arrTy = cast<ArrayType>(agg->getType());

    if (arrTy->isStaticallyConstrained())
        bounds = synthStaticArrayBounds(Builder, arrTy);
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
        const llvm::StructType *boundsTy = getType(arrTy);
        bounds = llvm::ConstantStruct::get(boundsTy, boundValues);
    }

    if (dst)
        Builder.CreateStore(bounds, dst);
    return bounds;
}
