//===-- typecheck/RangeChecker.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_RANGECHECKER_HDR_GUARD
#define COMMA_TYPECHECK_RANGECHECKER_HDR_GUARD

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines the RangeChecker class.
//===----------------------------------------------------------------------===//

#include "comma/basic/Diagnostic.h"

namespace comma {

class DiscreteType;
class Expr;
class Range;
class TypeCheck;

//===----------------------------------------------------------------------===//
// RangeChecker
//
/// \class
///
/// \brief RangeChecker provides several methods to analyze ranges.
class RangeChecker {

public:
    RangeChecker(TypeCheck &TC) : TC(TC) { }

    /// \brief Checks a range as used in an integer type declaration.
    ///
    /// The following method takes expressions denoting the lower and upper
    /// bounds of the range.  Semantic analysis performs the following checks:
    ///
    ///   - Resolves the lower and upper bounds to be in class integer with
    ///     preference for root_integer if there is an ambiguity.
    ///
    ///   - Ensures both the lower and upper bounds are static expressions.
    ///
    /// Note that these types of ranges do not require that the upper and lower
    /// bounds be of the same type.
    ///
    /// Returns true if the checks succeeded and false otherwise.
    bool checkDeclarationRange(Expr *lower, Expr *upper);

    /// \brief Checks a range as used as a loop control.
    ///
    /// The following method takes the expressions denoting the lower and upper
    /// bounds of a discrete subtype declaration (to be used as the control for
    /// a \c loop statement or as an index specification for an array type).
    /// Semantic analysis performs the following checks:
    ///
    ///   - Resolves the lower and upper bounds to be of discrete types with a
    ///     preference for root_integer if there is an ambiguity.
    ///
    ///   - Ensures both the upper and lower bounds resolve to the same type.
    ///
    ///   - If the type of the range resolves to root_integer, convert the
    ///     expressions to be of type Integer (ARM 3.6.18).
    ///
    /// Returns a discrete subtype constrained to the given bounds if the checks
    /// succeed and null otherwise.
    DiscreteType *checkDSTRange(Expr *lower, Expr *upper);

    /// \brief Checks a range for compatibility wrt a discrete type.
    ///
    /// Given a discrete subtype \t base, checks that the given bounds form a
    /// valid constraint over \t base.
    ///
    /// Returns a discrete subtype constrined to the given bounds if the check
    /// succeeds and null otherwise.
    DiscreteType *checkSubtypeRange(DiscreteType *base,
                                    Expr *lower, Expr *upper);

    /// Resolves the type of \p range to the given type.
    bool resolveRange(Range *range, DiscreteType *type);

private:
    TypeCheck &TC;              // Supporting TypeChecker instance.

    /// Posts the given diagnostic message.
    DiagnosticStream &report(Location loc, diag::Kind kind);
};

} // end comma namespace.

#endif
