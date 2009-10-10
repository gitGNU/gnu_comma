//===-- CheckAttrib.cpp --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AttribExpr.h"
#include "comma/ast/TypeRef.h"
#include "comma/typecheck/TypeCheck.h"

using namespace comma;
using llvm::dyn_cast_or_null;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

namespace {

/// The following class provides attribute checking services.
class AttributeChecker {

public:
    AttributeChecker(AstResource &resource, Diagnostic &diag,
                     attrib::AttributeID ID)
        : resource(resource), diagnostic(diag), ID(ID) { }

    Ast *checkAttribute(Ast *prefix, Location loc);

private:
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
    AttribExpr *checkScalarBound(TypeRef *prefix, Location loc);

    /// Helper for checkBound().  Handles array First and Last attributes.
    AttribExpr *checkArrayBound(Expr *prefix, Location loc);

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

AttribExpr *AttributeChecker::checkScalarBound(TypeRef *prefix, Location loc)
{
    // FIXME:  It is possible that the TypeRef is incomplete.  We should
    // diagnose that fact rather than ignore it.

    // When the prefix is a type, it must resolve to a scalar type.
    IntegerDecl *decl;

    decl = dyn_cast_or_null<IntegerDecl>(prefix->getTypeDecl());
    if (!decl) {
        report(loc, diag::ATTRIB_OF_NON_SCALAR) << attributeName();
        return 0;
    }

    if (ID == attrib::First)
        return new FirstAE(decl->getType(), loc);
    else
        return new LastAE(decl->getType(), loc);
}

AttribExpr *AttributeChecker::checkArrayBound(Expr *prefix, Location loc)
{
    ArraySubType *arrTy = dyn_cast<ArraySubType>(prefix->getType());

    if (!arrTy) {
        report(loc, diag::ATTRIB_OF_NON_ARRAY) << attributeName();
        return 0;
    }

    if (ID == attrib::First)
        return new FirstArrayAE(prefix, loc);
    else
        return new LastArrayAE(prefix, loc);
}

} // end anonymous namespace.


Ast *TypeCheck::checkAttribute(attrib::AttributeID ID,
                               Ast *prefix, Location loc)
{
    AttributeChecker AC(resource, diagnostic, ID);
    return AC.checkAttribute(prefix, loc);
}