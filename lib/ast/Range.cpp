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


Range::Range(Expr *lower, Expr *upper, DiscreteType *type)
    : Ast(AST_Range),
      lowerBound(lower, 0),
      upperBound(upper, 0),
      rangeTy(type)
{
    initializeStaticBounds();
}

Location Range::getLowerLocation() const
{
    return getLowerBound()->getLocation();
}

Location Range::getUpperLocation() const
{
    return getUpperBound()->getLocation();
}

void Range::initializeStaticBounds()
{
    Expr *lower = getLowerBound();
    Expr *upper = getUpperBound();

    // Try to evaluate the upper and lower bounds as static discrete
    // expressions and mark each bound as appropriate.
    if (lower->staticDiscreteValue(lowerValue))
        markLowerAsStatic();
    if (upper->staticDiscreteValue(upperValue))
        markUpperAsStatic();

    // If a type is associated with this range, resolve the width of the static
    // bounds (if available).
    if (hasType())
        checkAndAdjustLimits();
}

void Range::checkAndAdjustLimits()
{
    assert(rangeTy && "Range type not set!");

    unsigned width = rangeTy->getSize();
    if (rangeTy->isSigned())  {
        if (hasStaticLowerBound()) {
            assert(lowerValue.getMinSignedBits() <= width
                   && "Bounds too wide!");
            lowerValue.sextOrTrunc(width);
        }
        if (hasStaticUpperBound()) {
            assert(upperValue.getMinSignedBits() <= width
                   && "Bounds too wide!");
            upperValue.sextOrTrunc(width);
        }
    }
    else {
        if (hasStaticLowerBound()) {
            assert(lowerValue.getActiveBits() <= width
                   && "Bounds too wide!");
            lowerValue.zextOrTrunc(width);
        }
        if (hasStaticUpperBound()) {
            assert(upperValue.getActiveBits() <= width
                   && "Bounds too wide!");
            upperValue.zextOrTrunc(width);
        }
    }
}

void Range::setLowerBound(Expr *expr)
{
    lowerBound.setPointer(expr);
    if (expr->staticDiscreteValue(lowerValue)) {
        markLowerAsStatic();
        if (hasType())
            checkAndAdjustLimits();
    }
    else
        markLowerAsDynamic();
}

void Range::setUpperBound(Expr *expr)
{
    upperBound.setPointer(expr);
    if (expr->staticDiscreteValue(upperValue)) {
        markUpperAsStatic();
        if (hasType())
            checkAndAdjustLimits();
    }
    else
        markUpperAsDynamic();
}

bool Range::isNull() const
{
    if (!isStatic())
        return false;

    const llvm::APInt &lower = getStaticLowerBound();
    const llvm::APInt &upper = getStaticUpperBound();

    if (getType()->isSigned())
        return upper.slt(lower);
    else
        return upper.ult(lower);
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

    int64_t lower;
    int64_t upper;

    // Extract the bounds of this range according to the type.
    if (getType()->isSigned()) {
        lower = getStaticLowerBound().getSExtValue();
        upper = getStaticUpperBound().getSExtValue();
    }
    else {
        lower = getStaticLowerBound().getZExtValue();
        upper = getStaticUpperBound().getZExtValue();
    }

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
