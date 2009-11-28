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
/// \brief This file defines the AST representation of ranges.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_RANGE_HDR_GUARD
#define COMMA_AST_RANGE_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/PointerIntPair.h"

namespace comma {

//===----------------------------------------------------------------------===//
// Range
//
/// The Range class represents a subset of values belonging to some scalar type.
class Range : public Ast {

public:
    /// Constructs a Range given expressions for the lower and upper bounds and
    /// an overall type for the range.
    ///
    /// A range can be constructed without specifying a type.  However, without
    /// a type this class cannot compute a uniform reprepresentation for any
    /// static bounds.  More precisely, without a type, static bounds may have
    /// inconsistent bit widths.  Therefore, it is advisable to resolve the type
    /// of the range before performing calculations using any static bound
    /// expressions.
    ///
    /// \see hasType()
    /// \see setType()
    Range(Expr *lower, Expr *upper, DiscreteType *type = 0);

    /// Returns the location of the lower bound of this range.
    Location getLowerLocation() const;

    /// Returns the location of the upper bound of this range.
    Location getUpperLocation() const;

    /// Returns true if this range has a type associated with it.
    bool hasType() const { return rangeTy != 0; }

    /// Sets the type of this range.  This method will assert if a type has
    /// already been associated with this range.
    void setType(DiscreteType *type) {
        assert(!hasType() && "Range already associated with a type!");
        rangeTy = type;
        checkAndAdjustLimits();
    }

    //@{
    /// Returns the type of this range.
    DiscreteType *getType() { return rangeTy; }
    const DiscreteType *getType() const { return rangeTy; }
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

    /// Returns the number of values representable by this range.
    ///
    /// This method will assert if this is not a static range.
    uint64_t length() const;

    // Support isa/dyn_cast.
    static bool classof(const Range *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_Range;
    }

private:
    Range(const Range &);       // Do not implement.

    /// Attempts to evaluate the upper and lower expressions as static
    /// expressions.  If the type of the range is not set, the static values are
    /// potentially inconsistent wrt bit width.
    void initializeStaticBounds();

    /// If a bound is static, checks to ensure that the value falls within the
    /// representational limits of this ranges type.  Also, adjusts the value to
    /// a bit width that is inline with the type representation.  Again, the
    /// type of the range must be set before this method is called.
    void checkAndAdjustLimits();

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
    DiscreteType *rangeTy;      ///< the type of this range.
};

} // end comma namespace.

#endif
