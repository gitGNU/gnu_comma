//===-- typecheck/RangeChecker.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "RangeChecker.h"
#include "TypeCheck.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Range.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DiagnosticStream &RangeChecker::report(Location loc, diag::Kind kind)
{
    AstResource &resource = TC.getAstResource();
    SourceLocation sloc = resource.getTextProvider().getSourceLocation(loc);
    return TC.getDiagnostic().report(sloc, kind);
}

bool RangeChecker::checkDeclarationRange(Expr *lower, Expr *upper)
{
    // Resolve the lower and upper expressions to be in the integer class of
    // types.
    if (!TC.checkExprInContext(lower, Type::CLASS_Integer))
        return false;
    if (!TC.checkExprInContext(upper, Type::CLASS_Integer))
        return false;

    // Ensure both expressions are static.
    if (!(TC.ensureStaticIntegerExpr(lower) &&
          TC.ensureStaticIntegerExpr(upper)))
        return false;

    return true;
}

DiscreteType *RangeChecker::checkLoopRange(Expr *lower, Expr *upper)
{
    // If neither the lower or upper bound expressions have a type, evaluate
    // both the lower with the discrete classification as context, then evaluate
    // the upper bound wrt the resolved type.
    if (!lower->hasType() && !upper->hasType()) {
        if (!TC.checkExprInContext(lower, Type::CLASS_Discrete))
            return 0;
        if (!(upper = TC.checkExprInContext(upper, lower->getType())))
            return 0;
    }
    else if (lower->hasType() && !upper->hasType()) {
        if (!isa<DiscreteType>(lower->getType())) {
            report(lower->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
            return 0;
        }
        if (!(upper = TC.checkExprInContext(upper, lower->getType())))
            return 0;
    }
    else if (upper->hasType() && !lower->hasType()) {
        if (!isa<DiscreteType>(upper->getType())) {
            report(upper->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
            return 0;
        }
        if (!(lower = TC.checkExprInContext(lower, upper->getType())))
            return 0;
    }
    else {
        // Both bounds have a type.  Check for compatibility.
        if (!isa<DiscreteType>(lower->getType())) {
            report(lower->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
            return 0;
        }

        if (!isa<DiscreteType>(upper->getType())) {
            report(upper->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
            return 0;
        }

        if (!TC.covers(lower->getType(), upper->getType())) {
            report(lower->getLocation(), diag::INCOMPATIBLE_RANGE_TYPES);
            return 0;
        }
    }

    DiscreteType *rangeTy = cast<DiscreteType>(lower->getType());

    // If the type of range is root_integer, replace the lower and upper bounds
    // with conversion expressions to Integer.
    if (rangeTy == TC.getAstResource().getTheRootIntegerType()) {
        rangeTy = TC.getAstResource().getTheIntegerType();
        lower = new ConversionExpr(lower, rangeTy, lower->getLocation());
        upper = new ConversionExpr(upper, rangeTy, upper->getLocation());
    }

    // Form a constrained discrete subtype constrained by the given bounds.
    rangeTy = TC.getAstResource().createDiscreteSubtype(
        rangeTy->getIdInfo(), rangeTy, lower, upper);

    return rangeTy;
}

bool RangeChecker::resolveRange(Range *range, DiscreteType *type)
{
    assert(!range->hasType() && "Range already has a resolved type!");

    Expr *lower = range->getLowerBound();
    Expr *upper = range->getUpperBound();

    if (!(lower = TC.checkExprInContext(lower, type)) ||
        !(upper = TC.checkExprInContext(upper, type)))
        return false;

    range->setLowerBound(lower);
    range->setUpperBound(upper);
    range->setType(cast<DiscreteType>(lower->getType()));
    return true;
}
