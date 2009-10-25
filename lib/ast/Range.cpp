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


Range::Range(Expr *lower, Expr *upper)
    : Ast(AST_Range),
      lowerBound(lower, 0),
      upperBound(upper, 0)
{
    Type *lowerTy = lower->getType();
    Type *upperTy = upper->getType();

    // If either of the types are subtypes, reduce to each to the underlying
    // base.
    if (SubType *subTy = dyn_cast<SubType>(lowerTy))
        lowerTy = subTy->getTypeOf();
    if (SubType *subTy = dyn_cast<SubType>(upperTy))
        upperTy = subTy->getTypeOf();

    // Both expressions must be of the same common discrete type.
    assert(lowerTy == upperTy && "Type mismatch in range bounds!");
    assert(lowerTy->isDiscreteType() && "Non discrete type of range!");

    // Currently, the type must be an IntegerType.
    IntegerType *rangeTy = cast<IntegerType>(lowerTy);
    unsigned width = rangeTy->getBitWidth();

    // Try to evaluate the upper and lower bounds as static integer valued
    // expressions.  Mark each bound as appropriate and convert the CTC to a
    // width that matches this ranges type.
    if (lower->staticIntegerValue(lowerValue)) {
        markLowerAsStatic();
        assert(lowerValue.getMinSignedBits() <= width && "Bounds too wide!");
        lowerValue.sextOrTrunc(width);
    }
    if (upper->staticIntegerValue(upperValue)) {
        markUpperAsStatic();
        assert(upperValue.getMinSignedBits() <= width && "Bounds too wide!");
        upperValue.sextOrTrunc(width);
    }
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
