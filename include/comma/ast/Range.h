//===-- ast/Range.h ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file defines AST nodes which represent ranges and range
/// attributes.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_RANGE_HDR_GUARD
#define COMMA_AST_RANGE_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/PointerIntPair.h"

namespace comma {

/// The Range class represents a subset of values belonging to some scalar type.
class Range : public Ast {

public:
    /// Constructs a Range given expressions for the lower and upper bounds.
    /// The types of both bounds must be identical, for they determine the type
    /// of the Range itself.
    Range(Expr *lower, Expr *upper);

    /// Constructs a Range given expressions for the lower and upper bounds.
    /// The types of both bounds must be identical, for they determine the type
    /// of the Range itself.
    static Range *create(Expr *lower, Expr *upper) {
        return new Range(lower, upper);
    }

    //@{
    /// Returns the type of this range.  This is always a base scalar type.
    IntegerType *getType();
    const IntegerType *getType() const;
    //@}

    //@{
    /// Returns the expression defining the lower bound.
    Expr *getLowerBound() { return lowerBound.getPointer(); }
    const Expr *getLowerBound() const { return lowerBound.getPointer(); }
    //@}

    //@{
    /// Returns the expression defining the upper bound.
    Expr *getUpperBound() { return upperBound.getPointer(); }
    const Expr *getUpperBound() const { return upperBound.getPointer(); }
    //@}

    /// Returns true if the lower bound is static.
    bool hasStaticLowerBound() const { return lowerBound.getInt(); }

    /// Returns true if the upper bound is static.
    bool hasStaticUpperBound() const { return upperBound.getInt(); }

    /// Returns true if this Range is static.
    ///
    /// A static Range is one where both the lower and upper bounds are static
    /// scalar expressions.
    bool isStatic() const {
        return hasStaticLowerBound() && hasStaticUpperBound();
    }

    /// If this Range has a static lower bound, retrieve the value as an APInt.
    const llvm::APInt &getStaticLowerBound() const {
        assert(hasStaticLowerBound() && "Lower bound not static!");
        return lowerValue;
    }

    /// If this Range has a static upper bound, retrieve the value as an APInt.
    const llvm::APInt &getStaticUpperBound() const {
        assert(hasStaticUpperBound() && "Upper bound not static!");
        return upperValue;
    }

    /// Returns true if this is known to be a null range and false otherwise.
    /// Non-static ranges are never known to be null.
    bool isNull() const;

    /// Returns true if it is known that this range contains the given value.
    ///
    /// This Range must be static for this function to return true.
    bool contains(const llvm::APInt &value) const;

    // Support isa/dyn_cast.
    static bool classof(const Range *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_Range;
    }

private:
    Range(const Range &);       // Do not implement.

    /// Munges the low order bit of \c lowerBound to encode the fact that a
    /// static interpretaion of the value has been computed.
    void markLowerAsStatic() { lowerBound.setInt(1); }

    /// Munges the low order bit of \c upperBound to encode the fact that a
    /// static interpretaion of the value has been computed.
    void markUpperAsStatic() { upperBound.setInt(1); }

    typedef llvm::PointerIntPair<Expr *, 1> PtrInt;
    PtrInt lowerBound;          ///< lower bound expression.
    PtrInt upperBound;          ///< Upper bound expression.
    llvm::APInt lowerValue;     ///< static lower value.
    llvm::APInt upperValue;     ///< static upper value.
};

} // end comma namespace.

#endif
