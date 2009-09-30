//===-- typecheck/CheckExpr.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "Scope.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/TypeRef.h"
#include "comma/typecheck/TypeCheck.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

IndexedArrayExpr *TypeCheck::acceptIndexedArray(DeclRefExpr *ref,
                                                SVImpl<Expr*>::Type &indices)
{
    Location loc = ref->getLocation();
    ValueDecl *vdecl = ref->getDeclaration();
    ArrayType *arrTy = vdecl->getType()->getAsArrayType();

    if (!arrTy) {
        report(loc, diag::EXPECTED_ARRAY_FOR_INDEX);
        return 0;
    }

    // Check that the number of indices matches the rank of the array type.
    unsigned numIndices = indices.size();
    if (numIndices != arrTy->getRank()) {
        report(loc, diag::WRONG_NUM_SUBSCRIPTS_FOR_ARRAY);
        return 0;
    }

    // Ensure each index is compatible with the arrays type.  If an index does
    // not check, continue checking each remaining index.
    bool allOK = true;
    for (unsigned i = 0; i < numIndices; ++i) {
        Expr *index = indices[i];
        if (!checkExprInContext(index, arrTy->getIndexType(i)))
            allOK = false;
    }

    // If the indices did not all check out, do not build the expression.
    if (!allOK)
        return 0;

    // Create a DeclRefExpr to represent the array value and return a fresh
    // expression.
    DeclRefExpr *arrExpr = new DeclRefExpr(vdecl, loc);
    return new IndexedArrayExpr(arrExpr, &indices[0], numIndices);
}

// Typechecks the given expression in the given type context.  This method can
// update the expression (by resolving overloaded function calls, or assigning a
// type to an integer literal, for example).  Returns true if the expression was
// successfully checked.  Otherwise, false is returned and appropriate
// diagnostics are emitted.
bool TypeCheck::checkExprInContext(Expr *expr, Type *context)
{
    // Currently, only two types of expressions are "sensitive" to context,
    // meaning that we might need to patch up the AST so that it conforms to the
    // context -- IntegerLiterals and FunctionCallExpr's.
    if (IntegerLiteral *intLit = dyn_cast<IntegerLiteral>(expr))
        return resolveIntegerLiteral(intLit, context);
    if (FunctionCallExpr *fcall = dyn_cast<FunctionCallExpr>(expr))
        return resolveFunctionCall(fcall, context);

    Type *exprTy = expr->getType();
    assert(exprTy && "Expression does not have a resolved type!");

    if (covers(context, exprTy))
        return true;

    // FIXME: Need a better diagnostic here.
    report(expr->getLocation(), diag::INCOMPATIBLE_TYPES);
    return false;
}

// Resolves the type of the given integer literal, and ensures that the given
// type context is itself compatible with the literal provided.  Returns true if
// the literal was successfully checked.  Otherwise, false is returned and
// appropriate diagnostics are posted.
bool TypeCheck::resolveIntegerLiteral(IntegerLiteral *intLit, Type *context)
{
    if (intLit->hasType()) {
        assert(intLit->getType() == context &&
               "Cannot resolve literal to different type!");
        return true;
    }

    IntegerSubType *subtype = dyn_cast<IntegerSubType>(context);
    if (!subtype) {
        // FIXME: Need a better diagnostic here.
        report(intLit->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }

    // Unresolved integer literals are represented as signed APInts with a
    // minimal bit width (see acceptIntegerLiteral()).
    //
    // Since integer types are always signed, the literal is within the bounds
    // of the base type iff its width is less than or equal to the base types
    // width.  If the literal is in bounds, zero extend if needed to match the
    // base type.
    llvm::APInt &litValue = intLit->getValue();
    IntegerType *intTy = subtype->getAsIntegerType();
    unsigned targetWidth = intTy->getBitWidth();
    unsigned literalWidth = litValue.getBitWidth();
    if (literalWidth < targetWidth)
        litValue.zext(targetWidth);
    else if (literalWidth > targetWidth) {
        report(intLit->getLocation(), diag::VALUE_NOT_IN_RANGE_FOR_TYPE)
            << subtype->getIdInfo();
        return false;
    }

    // If the target subtype is constrained, check that the literal is in
    // bounds.  Note that the range bounds will be of the same width as the base
    // type since the "type of a range" is the "type of the subtype".
    if (subtype->isConstrained()) {
        RangeConstraint *range = subtype->getConstraint();
        if (!range->contains(litValue)) {
            report(intLit->getLocation(), diag::VALUE_NOT_IN_RANGE_FOR_TYPE)
                << subtype->getIdInfo();
        }
    }

    intLit->setType(context);
    return true;
}

Node TypeCheck::acceptInj(Location loc, Node exprNode)
{
    Domoid *domoid = getCurrentDomoid();

    if (!domoid) {
        report(loc, diag::INVALID_INJ_CONTEXT);
        return getInvalidNode();
    }

    // Check that the given expression is of the current domain type.
    DomainType *domTy = domoid->getPercentType();
    Expr *expr = cast_node<Expr>(exprNode);
    if (!checkExprInContext(expr, domTy))
        return getInvalidNode();

    // Check that the carrier type has been defined.
    CarrierDecl *carrier = domoid->getImplementation()->getCarrier();
    if (!carrier) {
        report(loc, diag::CARRIER_TYPE_UNDEFINED);
        return getInvalidNode();
    }

    exprNode.release();
    return getNode(new InjExpr(expr, carrier->getType(), loc));
}

Node TypeCheck::acceptPrj(Location loc, Node exprNode)
{
    Domoid *domoid = getCurrentDomoid();

    if (!domoid) {
        report(loc, diag::INVALID_PRJ_CONTEXT);
        return getInvalidNode();
    }

    // Check that the carrier type has been defined.
    CarrierDecl *carrier = domoid->getImplementation()->getCarrier();
    if (!carrier) {
        report(loc, diag::CARRIER_TYPE_UNDEFINED);
        return getInvalidNode();
    }

    Type *carrierTy = carrier->getRepresentationType();
    Expr *expr = cast_node<Expr>(exprNode);
    if (!checkExprInContext(expr, carrierTy))
        return getInvalidNode();

    exprNode.release();
    DomainType *prjType = domoid->getPercentType();
    return getNode(new PrjExpr(expr, prjType, loc));
}

Node TypeCheck::acceptIntegerLiteral(llvm::APInt &value, Location loc)
{
    // Integer literals are always unsigned (we have yet to apply a negation
    // operator, for example).   The parser is commited to procuding APInt's
    // which are no wider than necessary to represent the unsigned value.
    // Assert this property of the parser.
    assert((value == 0 || value.countLeadingZeros() == 0) &&
           "Unexpected literal representation!");

    // If non-zero, zero extend the literal by one bit.  Literals internally
    // represented as "minimally sized" signed integers until they are resolved
    // to a particular type.  This convetion simplifies the evaluation of
    // static integer expressions.
    if (value != 0)
        value.zext(value.getBitWidth() + 1);

    return getNode(new IntegerLiteral(value, loc));
}

Node TypeCheck::acceptStringLiteral(const char *string, unsigned len,
                                    Location loc)
{
    assert(false && "String literal support is not yet implemented!");
}
