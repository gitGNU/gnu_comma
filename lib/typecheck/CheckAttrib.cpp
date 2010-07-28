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

    Expr *resolveAttribute(AttribExpr *attrib, Type *context);

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

    /// Checks a Length attribute applied to an array.
    Expr *checkLength(Ast *prefix, Location loc);

    /// Resolves \p expr to an array valued expression.
    ///
    /// This is a helper method for various attribute checkers which require an
    /// array valued prefix.  This method performs the following operations:
    ///
    ///   - If \p expr does not have a resolved type, check it in the class wide
    ///     context Type::CLASS_Array.
    ///
    ///   - If \p expr is resolved but does not have array type attempt to
    ///     dereference \p expr.
    ///
    /// Returns the updated expression node on success.
    Expr *resolveArrayType(Expr *expr, Location loc);

    SourceLocation getSourceLoc(Location loc) const {
        return TC.getTextManager().getSourceLocation(loc);
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

    case attrib::Length:
        result = checkLength(prefix, loc);
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
    // Resolve the prefix to a scalar type.
    TypeDecl *decl = prefix->getDecl();
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
    if (!(prefix = resolveArrayType(prefix, loc)))
        return 0;

    if (ID == attrib::First)
        return new FirstArrayAE(prefix, loc);
    else
        return new LastArrayAE(prefix, loc);
}

RangeAttrib *AttributeChecker::checkRange(Ast *prefix, Location loc)
{
    // If the prefix denotes an expression it must resolve to an array type.
    if (Expr *expr = dyn_cast<Expr>(prefix)) {
        if ((expr = resolveArrayType(expr, loc)))
            return new ArrayRangeAttrib(expr, loc);
        return 0;
    }

    // Otherwise, the prefix must be a TypeRef resolving to a scalar type.
    TypeRef *ref = dyn_cast<TypeRef>(prefix);

    if (!ref) {
        report(loc, diag::INVALID_ATTRIB_PREFIX) << attributeName();
        return 0;
    }

    TypeDecl *tyDecl = ref->getDecl();
    DiscreteType *prefixTy = dyn_cast<DiscreteType>(tyDecl->getType());

    if (!prefixTy) {
        report(loc, diag::INVALID_ATTRIB_PREFIX) << attributeName();
        return 0;
    }

    delete ref;
    return new ScalarRangeAttrib(prefixTy, loc);
}

SubroutineRef *AttributeChecker::checkPosVal(Ast *prefix, Location loc)
{
    assert((ID == attrib::Pos || ID == attrib::Val) &&
           "Unexpected attribute ID!");

    TypeRef *ref = dyn_cast<TypeRef>(prefix);

    if (!ref) {
        report(prefix->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
        return 0;
    }

    TypeDecl *tyDecl = ref->getDecl();
    FunctionAttribDecl *attrib;

    if (IntegerDecl *intDecl = dyn_cast<IntegerDecl>(tyDecl))
        attrib = intDecl->getAttribute(ID);
    else if (EnumerationDecl *enumDecl = dyn_cast<EnumerationDecl>(tyDecl))
        attrib = enumDecl->getAttribute(ID);
    else {
        report(prefix->getLocation(), diag::EXPECTED_DISCRETE_SUBTYPE);
        return 0;
    }

    delete ref;
    return new SubroutineRef(loc, attrib);
}

Expr *AttributeChecker::checkLength(Ast *prefix, Location loc)
{
    Expr *expr = TC.ensureExpr(prefix);

    if (!expr)
        return 0;

    if (!(expr = resolveArrayType(expr, loc)))
        return 0;

    return new LengthAE(expr, loc);
}

Expr *AttributeChecker::resolveArrayType(Expr *expr, Location loc)
{
    if (!expr->hasResolvedType()) {
        if (!TC.checkExprInContext(expr, Type::CLASS_Array))
            return 0;
    }

    Type *exprTy = expr->getType();

    if (exprTy->isArrayType())
        return expr;

    if ((exprTy = TC.getCoveringDereference(exprTy, Type::CLASS_Array)))
        return TC.implicitlyDereference(expr, exprTy);

    report(loc, diag::ATTRIB_OF_NON_ARRAY) << attributeName();
    return 0;
}

Expr *AttributeChecker::resolveAttribute(AttribExpr *attrib, Type *context)
{
    if (attrib->hasResolvedType()) {
        assert(TC.covers(attrib->getType(), context) &&
               "Cannot resolve attribute to different type!");
        return attrib;
    }

    // Currently, the only type of attribute expression which does not have a
    // fully resolved type associated with it is a Length attribute.
    Location loc = attrib->getLocation();
    LengthAE *length = cast<LengthAE>(attrib);
    IntegerType *subtype = dyn_cast<IntegerType>(context);
    unsigned dimension = length->getDimension();

    // FIXME: The following code is a near copy of TC::resolveIntegerLiteral.
    // Refactor.
    if (!subtype) {
        // FIXME: Need a better diagnostic here.
        report(loc, diag::INCOMPATIBLE_TYPES);
        return 0;
    }

    unsigned targetWidth = subtype->getSize();
    unsigned literalWidth =
        dimension ? 32 - llvm::CountLeadingZeros_32(dimension) : 1;
    llvm::APInt dimValue(literalWidth, dimension);

    if (literalWidth < targetWidth)
        dimValue.sext(targetWidth);
    else if (literalWidth > targetWidth) {
        report(loc, diag::VALUE_NOT_IN_RANGE_FOR_TYPE) << subtype->getIdInfo();
        return 0;
    }

    DiscreteType::ContainmentResult containment = subtype->contains(dimValue);

    // If the value is contained by the context type simply set the type of the
    // attribute to the context and return.
    if (containment == DiscreteType::Is_Contained) {
        length->setType(context);
        return length;
    }

    // If the value is definitely not contained by the context return null.
    if (containment == DiscreteType::Not_Contained) {
        report(loc, diag::VALUE_NOT_IN_RANGE_FOR_TYPE) << subtype->getIdInfo();
        return 0;
    }

    // Otherwise check that the value is representable as root_integer and, if
    // so, set its type to root_integer and wrap it in a conversion expression.
    //
    // FIXME: It would probably be better to check if the value fits within the
    // base type of the context.  This would be more efficient for codegen as it
    // would remove unnecessary truncations.
    IntegerType *rootTy = TC.getAstResource().getTheRootIntegerType();
    containment = rootTy->contains(dimValue);

    if (containment == DiscreteType::Not_Contained) {
        report(loc, diag::VALUE_NOT_IN_RANGE_FOR_TYPE) << subtype->getIdInfo();
        return 0;
    }

    length->setType(rootTy);
    return new ConversionExpr(length, context, loc, true);
}

} // end anonymous namespace.


Ast *TypeCheck::checkAttribute(attrib::AttributeID ID,
                               Ast *prefix, Location loc)
{
    AttributeChecker AC(*this, ID);
    return AC.checkAttribute(prefix, loc);
}

Expr *TypeCheck::resolveAttribExpr(AttribExpr *attrib, Type *context)
{
    AttributeChecker AC(*this, attrib->getAttributeID());
    return AC.resolveAttribute(attrib, context);
}
