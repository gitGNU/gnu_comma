//===-- ast/Range.cpp ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/Range.h"

#include "llvm/ADT/APInt.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


Range *Range::create(Expr *lower, Expr *upper)
{
    Type *lowerTy = lower->getType();
    Type *upperTy = upper->getType();

    // If either of the types are subtypes, reduce to each to the underlying
    // base.
    if (SubType *subTy = dyn_cast<SubType>(lowerTy))
        lowerTy = subTy->getTypeOf();
    if (SubType *subTy = dyn_cast<SubType>(upperTy))
        upperTy = subTy->getTypeOf();

    // Both expressions must be of the same common type.
    assert(lowerTy == upperTy && "Type mismatch in array bounds!");

    // Currently, the type must be an IntegerType.
    IntegerType *rangeTy = cast<IntegerType>(lowerTy);

    llvm::APInt lowerVal;
    llvm::APInt upperVal;

    bool staticLower = lower->staticIntegerValue(lowerVal);
    bool staticUpper = upper->staticIntegerValue(upperVal);
    unsigned width = rangeTy->getBitWidth();

    // We need to make the width of the bounds the same as the width of the base
    // type.  Ensure that we are not loosing any significant bits.
    assert(lowerVal.getMinSignedBits() <= width && "Bounds too wide!");
    assert(upperVal.getMinSignedBits() <= width && "Bounds too wide!");

    lowerVal.sextOrTrunc(width);
    upperVal.sextOrTrunc(width);

    // Compute the size of the actual object.
    size_t size = sizeof(Range);
    if (staticLower)
        size += sizeof(llvm::APInt);
    if (staticUpper)
        size += sizeof(llvm::APInt);

    // Allocate a region to hold the final representaion.
    char *buff = new char[size];

    // Initialize the range.
    Range *range = new (buff) Range(lower, upper);

    // Initialize each bound, if needed.
    buff += sizeof(Range);
    if (staticLower) {
        new (buff) llvm::APInt(lowerVal);
        range->markLowerAsStatic();
        buff += sizeof(llvm::APInt);
    }
    if (staticUpper) {
        new (buff) llvm::APInt(upperVal);
        range->markUpperAsStatic();
    }
    return range;
}

IntegerType *Range::getType()
{
    Type *Ty = getLowerBound()->getType();

    if (SubType *subTy = dyn_cast<SubType>(Ty))
        Ty = subTy->getTypeOf();

    // Currently, all range types are integer types.
    return cast<IntegerType>(Ty);
}

const IntegerType *Range::getType() const
{
    const Type *Ty = getLowerBound()->getType();

    if (const SubType *subTy = dyn_cast<SubType>(Ty))
        Ty = subTy->getTypeOf();

    // Currently, all range types are integer types.
    return cast<IntegerType>(Ty);
}

const llvm::APInt &Range::getStaticLowerBound() const
{
    assert(hasStaticLowerBound());
    const llvm::APInt *value = reinterpret_cast<const llvm::APInt*>(this + 1);
    return *value;
}

const llvm::APInt &Range::getStaticUpperBound() const
{
    assert(hasStaticUpperBound());

    const llvm::APInt *value = reinterpret_cast<const llvm::APInt*>(this + 1);

    if (hasStaticLowerBound())
        value++;
    return *value;
}

bool Range::isNull() const
{
    if (!isStatic())
        return false;

    const llvm::APInt &lower = getStaticLowerBound();
    const llvm::APInt &upper = getStaticUpperBound();

    return upper.slt(lower);
}

bool Range::contains(const llvm::APInt &value) const
{
    if (!isStatic())
        return false;

    if (isNull())
        return false;

    llvm::APInt candidate(value);

    unsigned rangeWidth = getType()->getBitWidth();

    if (candidate.getMinSignedBits() > rangeWidth)
        return false;
    else if (candidate.getBitWidth() > rangeWidth)
        candidate.trunc(rangeWidth);
    else if (candidate.getBitWidth() < rangeWidth)
        candidate.sext(rangeWidth);

    const llvm::APInt &lower = getStaticLowerBound();
    const llvm::APInt &upper = getStaticUpperBound();

    return lower.sle(candidate) && candidate.sle(upper);
}
