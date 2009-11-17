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

    // If either of the types are primary, reduce to each to the underlying
    // root.
    if (PrimaryType *primary = dyn_cast<PrimaryType>(lowerTy))
        lowerTy = primary->getRootType();
    if (PrimaryType *primary = dyn_cast<PrimaryType>(upperTy))
        upperTy = primary->getRootType();

    // Both expressions must be of the same common discrete type.
    assert(lowerTy == upperTy && "Type mismatch in range bounds!");
    assert(lowerTy->isDiscreteType() && "Non discrete type of range!");

    // FIXME: Currently, the type must be a discrete type.  We should allow any
    // scalar type here.
    DiscreteType *rangeTy = cast<DiscreteType>(lowerTy);
    unsigned width = rangeTy->getSize();

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

DiscreteType *Range::getType()
{
    Type *Ty = getLowerBound()->getType();

    if (PrimaryType *primary = dyn_cast<PrimaryType>(Ty))
        Ty = primary->getRootType();

    // Currently, all range types have a discrete type.
    return cast<DiscreteType>(Ty);
}

const DiscreteType *Range::getType() const
{
    const Type *Ty = getLowerBound()->getType();

    if (const PrimaryType *primary = dyn_cast<PrimaryType>(Ty))
        Ty = primary->getRootType();

    // Currently, all range types have a discrete type.
    return cast<DiscreteType>(Ty);
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
    // FIXME: Perhaps this method should return a ternary value instead of a
    // bool.
    assert(isStatic() && "Cannot determin containment for non-static ranges!");

    if (isNull())
        return false;

    llvm::APInt candidate(value);

    unsigned rangeWidth = getType()->getSize();

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

uint64_t Range::length() const
{
    assert(isStatic() && "Cannot compute length of non-static range!");

    if (isNull())
        return 0;

    int64_t lower = getStaticLowerBound().getSExtValue();
    int64_t upper = getStaticUpperBound().getSExtValue();

    if (lower < 0) {
        uint64_t lowElems = -lower;
        if (upper >= 0)
            return uint64_t(upper) + lowElems + 1;
        else {
            uint64_t upElems = -upper;
            return lowElems - upElems + 1;
        }
    }

    // The range is non-null, so upper > 0 and lower >= 0.
    return uint64_t(upper - lower) + 1;
}
