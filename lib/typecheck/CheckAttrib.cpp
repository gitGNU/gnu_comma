//===-- CheckAttrib.cpp --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "TypeCheck.h"
#include "comma/ast/AttribDecl.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/TypeRef.h"

using namespace comma;
using llvm::dyn_cast_or_null;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// The following class provides attribute checking services.
class AttributeChecker {

public:
    AttributeChecker(TypeCheck &TC, attrib::AttributeID ID)
        : TC(TC),
          resource(TC.getAstResource()),
          diagnostic(TC.getDiagnostic()),
          ID(ID) { }

    Ast *checkAttribute(Ast *prefix, Location loc);

private:
    TypeCheck &TC;
    AstResource &resource;
    Diagnostic &diagnostic;
    attrib::AttributeID ID;

    /// Returns the name of the attribute ID.
    const char *attributeName() {
        return attrib::getAttributeString(ID);
    }

    /// Checks the attributes First and Last.
    ///
    /// \param prefix An arbitrary ast node representing the prefix of the
    /// attribute.
    ///
    /// \param loc the location of the attribute (as opposed to the location of
    /// its prefix).
    ///
    /// \return An AttribExpr representing the attribute, or null on failure.
    /// In the latter case, diagnostics are posted.
    AttribExpr *checkBound(Ast *prefix, Location loc);

    /// Helper for checkBound().  Handles scalar First and Last attributes.
    ScalarBoundAE *checkScalarBound(TypeRef *prefix, Location loc);

    /// Helper for checkBound().  Handles array First and Last attributes.
    ArrayBoundAE *checkArrayBound(Expr *prefix, Location loc);

    /// Checks a range attribute.
    RangeAttrib *checkRange(Ast *prefix, Location loc);

    /// Checks a Pos or Val attribute and returns a SubroutineRef pointing at
    /// the corresponding declaration or null if the prefix is not compatible
    /// with such an attribute.
    SubroutineRef *checkPosVal(Ast *prefix, Location loc);

    SourceLocation getSourceLoc(Location loc) const {
        return resource.getTextProvider().getSourceLocation(loc);
    }

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        return diagnostic.report(getSourceLoc(loc), kind);
    }
};

Ast *AttributeChecker::checkAttribute(Ast *prefix, Location loc)
{
    Ast *result = 0;

    switch (ID) {
    default:
        assert(false && "Unknown attribute!");
        break;

    case attrib::First:
    case attrib::Last:
        result = checkBound(prefix, loc);
        break;

    case attrib::Pos:
    case attrib::Val:
        result = checkPosVal(prefix, loc);
        break;

    case attrib::Range:
        result = checkRange(prefix, loc);
        break;
    };
    return result;
}

AttribExpr *AttributeChecker::checkBound(Ast *prefix, Location loc)
{
    AttribExpr *result = 0;

    if (TypeRef *ref = dyn_cast<TypeRef>(prefix))
        result = checkScalarBound(ref, loc);
    else if (Expr *expr = dyn_cast<Expr>(prefix))
        result = checkArrayBound(expr, loc);
    else {
        // FIXME: The location here is of the attribute, not the prefix.  The
        // AST should provide a service similar to TypeCheck::getNodeLoc.
        report(loc, diag::INVALID_ATTRIB_PREFIX) << attributeName();
    }
    return result;
}

ScalarBoundAE *AttributeChecker::checkScalarBound(TypeRef *prefix, Location loc)
{
    // FIXME:  It is possible that the TypeRef is incomplete.  We should
    // diagnose that fact rather than ignore it.

    // When the prefix is a type, it must resolve to a scalar type.
    TypeDecl *decl = prefix->getTypeDecl();
    DiscreteType *prefixTy = dyn_cast<DiscreteType>(decl->getType());

    if (!decl) {
        report(loc, diag::ATTRIB_OF_NON_SCALAR) << attributeName();
        return 0;
    }

    if (ID == attrib::First)
        return new FirstAE(prefixTy, loc);
    else
        return new LastAE(prefixTy, loc);
}

ArrayBoundAE *AttributeChecker::checkArrayBound(Expr *prefix, Location loc)
{
    ArrayType *arrTy = dyn_cast<ArrayType>(prefix->getType());

    if (!arrTy) {
        report(loc, diag::ATTRIB_OF_NON_ARRAY) << attributeName();
        return 0;
    }

    if (ID == attrib::First)
        return new FirstArrayAE(prefix, loc);
    else
        return new LastArrayAE(prefix, loc);
}

RangeAttrib *AttributeChecker::checkRange(Ast *prefix, Location loc)
{
    // If the prefix denotes an expression it must resolve to an array type
    // including any implicit dereferencing.
    if (Expr *expr = dyn_cast<Expr>(prefix)) {
        if (!(expr->hasResolvedType() ||
              TC.checkExprInContext(expr, Type::CLASS_Array)))
            return 0;

        if (!isa<ArrayType>(expr->getType())) {
            Type *type = expr->getType();
            if ((type = TC.getCoveringDereference(type, Type::CLASS_Array)))
                expr = TC.implicitlyDereference(expr, type);
            else {
                report(loc, diag::ATTRIB_OF_NON_ARRAY) << attributeName();
                return 0;
            }
        }
        return new ArrayRangeAttrib(expr, loc);
    }

    // Otherwise, the prefix must be a TypeRef resolving to a scalar type.
    TypeRef *ref = dyn_cast<TypeRef>(prefix);

    if (!ref || !ref->referencesTypeDecl()) {
        report(loc, diag::INVALID_ATTRIB_PREFIX) << attributeName();
        return 0;
    }

    TypeDecl *tyDecl = ref->getTypeDecl();
    DiscreteType *prefixTy = dyn_cast<DiscreteType>(tyDecl->getType());

    if (!prefixTy) {
        report(loc, diag::INVALID_ATTRIB_PREFIX) << attributeName();
        return 0;
    }

    return new ScalarRangeAttrib(prefixTy, loc);
}

SubroutineRef *AttributeChecker::checkPosVal(Ast *prefix, Location loc)
{
    assert((ID == attrib::Pos || ID == attrib::Val) &&
           "Unexpected attribute ID!");

    TypeRef *ref = dyn_cast<TypeRef>(prefix);

    if (!(ref && ref->referencesTypeDecl())) {
        report(prefix->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
        return 0;
    }

    TypeDecl *tyDecl = ref->getTypeDecl();
    FunctionAttribDecl *attrib;

    if (IntegerDecl *intDecl = dyn_cast<IntegerDecl>(tyDecl))
        attrib = intDecl->getAttribute(ID);
    else if (EnumerationDecl *enumDecl = dyn_cast<EnumerationDecl>(tyDecl))
        attrib = enumDecl->getAttribute(ID);

    if (!attrib) {
        report(prefix->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
        return 0;
    }

    return new SubroutineRef(loc, attrib);
}

} // end anonymous namespace.


Ast *TypeCheck::checkAttribute(attrib::AttributeID ID,
                               Ast *prefix, Location loc)
{
    AttributeChecker AC(*this, ID);
    return AC.checkAttribute(prefix, loc);
}
