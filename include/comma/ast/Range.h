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

namespace llvm {

class APInt;

} // end llvm namespace;

namespace comma {

/// The Range class represents a subset of values belonging to some scalar type.
class Range : public Ast {

public:
    /// Constructs a Range given expressions for the lower and upper bounds.
    /// The types of both bounds must be identical, for they determine the type
    /// of the Range itself.
    static Range *create(Expr *lower, Expr *upper);

    //@{
    /// Returns the type of this range.  This is always a base scalar type.
    IntegerType *getType();
    const IntegerType *getType() const;
    //@}

    //@{
    /// Returns the expression defining the lower bound.
    Expr *getLowerBound() {
        intptr_t bits = getIntPtr(lowerBound) & ~uintptr_t(1);
        return reinterpret_cast<Expr*>(bits);
    }
    const Expr *getLowerBound() const {
        intptr_t bits = getIntPtr(lowerBound) & ~uintptr_t(1);
        return reinterpret_cast<Expr*>(bits);
    }
    //@}

    //@{
    /// Returns the expression defining the upper bound.
    Expr *getUpperBound() {
        intptr_t bits = getIntPtr(upperBound) & ~uintptr_t(1);
        return reinterpret_cast<Expr*>(bits);
    }
    const Expr *getUpperBound() const {
        intptr_t bits = getIntPtr(upperBound) & ~uintptr_t(1);
        return reinterpret_cast<Expr*>(bits);
    }
    //@}

    /// Returns true if the lower bound is static.
    bool hasStaticLowerBound() const {
        return getIntPtr(lowerBound) & 1;
    }

    /// Returns true if the upper bound is static.
    bool hasStaticUpperBound() const {
        return getIntPtr(upperBound) & 1;
    }

    /// Returns true if this Range is static.
    ///
    /// A static Range is one where both the lower and upper bounds are static
    /// scalar expressions.
    bool isStatic() const {
        return hasStaticLowerBound() && hasStaticUpperBound();
    }

    /// If this Range has a static lower bound, retrieve the value as an APInt.
    const llvm::APInt &getStaticLowerBound() const;

    /// If this Range has a static upper bound, retrieve the value as an APInt.
    const llvm::APInt &getStaticUpperBound() const;

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
    Range(Expr *lower, Expr *upper)
        : Ast(AST_Range),
          lowerBound(lower),
          upperBound(upper) { }

    Range(const Range &);       // Do not implement.

    /// Converts an Expr to an intptr_t.
    static intptr_t getIntPtr(Expr *ptr) {
        return reinterpret_cast<intptr_t>(ptr);
    }

    /// Converts an intptr_t to an Expr.
    static Expr *getPtrInt(intptr_t bits) {
        return reinterpret_cast<Expr*>(bits);
    }

    /// Munges the low order bit of \c lowerBound to encode the fact that a
    /// static interpretaion of the value has been computed.
    void markLowerAsStatic() {
        intptr_t bits = getIntPtr(lowerBound) | intptr_t(1);
        lowerBound = getPtrInt(bits);
    }

    /// Munges the low order bit of \c upperBound to encode the fact that a
    /// static interpretaion of the value has been computed.
    void markUpperAsStatic() {
        intptr_t bits = getIntPtr(upperBound) | intptr_t(1);
        upperBound = getPtrInt(bits);
    }

    Expr *lowerBound;           ///< lower bound expression.
    Expr *upperBound;           ///< Upper bound expression.
};

} // end comma namespace.

#endif
