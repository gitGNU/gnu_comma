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

#include <algorithm>

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
    // Three types of expressions are "sensitive" to context, meaning that we
    // might need to patch up the AST so that it conforms to the context --
    // FunctionCallExpr, IntegerLiteral, and StringLiteral.
    if (FunctionCallExpr *fcall = dyn_cast<FunctionCallExpr>(expr))
        return resolveFunctionCall(fcall, context);
    if (IntegerLiteral *intLit = dyn_cast<IntegerLiteral>(expr))
        return resolveIntegerLiteral(intLit, context);
    if (StringLiteral *strLit = dyn_cast<StringLiteral>(expr))
        return resolveStringLiteral(strLit, context);

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

namespace {

/// Helper function for acceptStringLiteral().  Extracts the enumeration
/// declarations resulting from the lookup of a character literal.
void getVisibleEnumerations(Resolver &resolver,
                            llvm::SmallVectorImpl<EnumerationDecl*> &enums)
{
    typedef llvm::SmallVector<SubroutineDecl*, 8> RoutineBuff;
    RoutineBuff routines;

    resolver.getVisibleSubroutines(routines);

    RoutineBuff::iterator I = routines.begin();
    RoutineBuff::iterator E = routines.end();
    for ( ; I != E; ++I) {
        EnumLiteral *lit = cast<EnumLiteral>(*I);
        enums.push_back(lit->getDeclRegion());
    }
}

/// Helper function for acceptStringLiteral().  Forms the intersection of
/// component enumeration types for a string literal.
void intersectComponentTypes(StringLiteral *string,
                             llvm::SmallVectorImpl<EnumerationDecl*> &enums)
{
    typedef StringLiteral::component_iterator iterator;
    iterator I = string->begin_component_types();
    while (I != string->end_component_types()) {
        EnumerationDecl *decl = *I;
        ++I;
        if (std::find(enums.begin(), enums.end(), decl) == enums.end())
            string->removeComponentType(decl);
    }
}

} // end anonymous namespace.

Node TypeCheck::acceptStringLiteral(const char *chars, unsigned len,
                                    Location loc)
{
    // The parser provides us with a string which includes the quotation marks.
    // This means that the given length is at least 2.  Our internal
    // representation drops the outer quotes.
    assert(len >= 2 && chars[0] == '"' && chars[len - 1] == '"' &&
           "Unexpected string format!");

    const char *I = chars + 1;
    const char *E = chars + len - 1;
    StringLiteral *string = new StringLiteral(I, E, loc);

    char buff[3] = { '\'', 0, '\'' };
    typedef llvm::SmallVector<EnumerationDecl*, 8> LitVec;

    while (I != E) {
        buff[1] = *I;
        IdentifierInfo *id = resource.getIdentifierInfo(&buff[0], 3);
        Resolver &resolver = scope->getResolver();
        LitVec literals;

        ++I;
        resolver.resolve(id);
        getVisibleEnumerations(resolver, literals);

        // We should always have a set of visible subroutines.  Character
        // literals are modeled as functions with "funny" names (therefore, they
        // cannot conflict with any other kind of declaration), and the language
        // defined character types are always visible (unless hidden by a
        // character declaration of the name name).
        assert(!literals.empty() && "Failed to resolve character literal!");

        // If the string literal has zero interpretaions this must be the first
        // character in the string.  Add each resolved declaration.
        if (string->zeroComponentTypes()) {
            string->addComponentTypes(literals.begin(), literals.end());
            continue;
        }

        // Form the intersection of the current component types with the
        // resolved types.
        intersectComponentTypes(string, literals);

        // The result of the interesction should never be zero since the
        // language defined character types supply declarations for all possible
        // literals.
        assert(!string->zeroComponentTypes() && "Disjoint character types!");
    }

    return getNode(string);
}

bool TypeCheck::resolveStringLiteral(StringLiteral *strLit, Type *context)
{
    // First, ensure the type context is a string array type.
    ArraySubType *arrTy = dyn_cast<ArraySubType>(context);
    if (!arrTy || !arrTy->isStringType()) {
        report(strLit->getLocation(), diag::INCOMPATIBLE_TYPES);
        return false;
    }

    // The array is a string type.  Check that the string literal has at least
    // one interpretation of its components which matches the component type of
    // the target.
    //
    // FIXME: more work needs to be done here when the enumeration type is
    // constrained.
    EnumSubType *enumTy = cast<EnumSubType>(arrTy->getComponentType());
    if (!strLit->containsComponentType(enumTy)) {
        report(strLit->getLocation(), diag::STRING_COMPONENTS_DO_NOT_SATISFY)
            << enumTy->getIdInfo();
        return false;
    }

    // If the array type is statically constrained, ensure that the string is of
    // the proper width.  Currently, all constrained array indices are
    // statically constrained.
    if (arrTy->isConstrained()) {
        uint64_t arrLength = arrTy->length();
        uint64_t strLength = strLit->length();

        if (arrLength < strLength) {
            report(strLit->getLocation(), diag::TOO_MANY_ELEMENTS_FOR_TYPE)
                << arrTy->getIdInfo();
            return false;
        }
        if (arrLength > strLength) {
            report(strLit->getLocation(), diag::TOO_FEW_ELEMENTS_FOR_TYPE)
                << arrTy->getIdInfo();
            return false;
        }

        /// Resolve the component type of the literal to the component type of
        /// the array and set the type of the literal to the type of the array.
        strLit->resolveComponentType(enumTy);
        strLit->setType(arrTy);
        return true;
    }

    assert(false && "Unconstrained array types are not yet supported!");
    return false;
}
