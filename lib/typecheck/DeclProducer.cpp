//===-- typecheck/DeclProducer.cpp ---------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// This file implements the typecheck methods responsible for the production of
// the implicit declarations required by certain type definitions.
//
//===----------------------------------------------------------------------===//

#include "DeclProducer.h"
#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Type.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

DeclProducer::DeclProducer(AstResource *resource)
    : resource(resource),
      theBoolDecl(0),
      theIntegerDecl(0)
{
    createTheBoolDecl();
    createTheIntegerDecl();

    createImplicitDecls(theBoolDecl);
    createImplicitDecls(theIntegerDecl);
}

/// Constructor method for producing a raw Bool decl.  This function does not
/// generate the associated implicit functions, however, the literals True and
/// False are produced.
void DeclProducer::createTheBoolDecl()
{
    IdentifierInfo *boolId = resource->getIdentifierInfo("Bool");
    IdentifierInfo *trueId = resource->getIdentifierInfo("true");
    IdentifierInfo *falseId = resource->getIdentifierInfo("false");

    // Declaration order of the True and False enumeration literals is critical.
    // We need False defined first so that it codegens to 0.
    theBoolDecl = new EnumerationDecl(boolId, 0, 0);
    new EnumLiteral(theBoolDecl, falseId, 0);
    new EnumLiteral(theBoolDecl, trueId, 0);
}

/// Constructor method for producing a raw Integer decl.  This function does not
/// generate the associated implicit functions.
void DeclProducer::createTheIntegerDecl()
{
    IdentifierInfo *integerId = resource->getIdentifierInfo("Integer");

    // FIXME:  The following is obviously target dependent.  For now, assume
    // that we are targeting x86-64.
    llvm::APInt lowVal(64, 1UL << 63);
    llvm::APInt highVal(64, ~(1UL << 63));
    IntegerLiteral *lowExpr = new IntegerLiteral(lowVal, 0);
    IntegerLiteral *highExpr = new IntegerLiteral(highVal, 0);
    IntegerType *intTy = resource->getIntegerType(lowVal, highVal);

    theIntegerDecl = new IntegerDecl(integerId, 0, lowExpr, highExpr, intTy, 0);
}


/// Returns the unique enumeration decl representing Bool.
EnumerationDecl *DeclProducer::getBoolDecl() const
{
    return theBoolDecl;
}

/// Returns the unique enumeration type representing Bool.
EnumerationType *DeclProducer::getBoolType() const
{
    return theBoolDecl->getType();
}

/// Returns the unique integer decl representing Integer.
IntegerDecl *DeclProducer::getIntegerDecl() const
{
    return theIntegerDecl;
}

/// Returns the unique TypedefType representing Integer.
TypedefType *DeclProducer::getIntegerType() const
{
    return theIntegerDecl->getType();
}

/// Returns an IdentifierInfo nameing the given predicate.
IdentifierInfo *DeclProducer::getPredicateName(PredicateKind kind)
{
    switch (kind) {
    default:
        assert(false && "Bad kind of predicate!");
        return 0;
    case EQ_pred:
        return resource->getIdentifierInfo("=");
    case LT_pred:
        return resource->getIdentifierInfo("<");
    case GT_pred:
        return resource->getIdentifierInfo(">");
    case LTEQ_pred:
        return resource->getIdentifierInfo("<=");
    case GTEQ_pred:
        return resource->getIdentifierInfo(">=");
    }
}

/// Returns the primitive operation marker for the given predicate.
PO::PrimitiveID DeclProducer::getPredicatePrimitive(PredicateKind kind)
{
    switch (kind) {
    default:
        assert(false && "Bad kind of predicate!");
        return PO::NotPrimitive;
    case EQ_pred:
        return PO::Equality;
    case LT_pred:
        return PO::LessThan;
    case GT_pred:
        return PO::GreaterThan;
    case LTEQ_pred:
        return PO::LessThanOrEqual;
    case GTEQ_pred:
        return PO::GreaterThanOrEqual;
    }
}

/// Generates declarations appropriate for the given enumeration, populating \p
/// enumDecl viewed as a DeclRegion with the results.
void DeclProducer::createImplicitDecls(EnumerationDecl *enumDecl)
{
    // Construct the builtin equality function.
    FunctionDecl *equals =
        createPredicate(EQ_pred, enumDecl->getType(), enumDecl);
    enumDecl->addDecl(equals);
}

/// Generates declarations appropriate for the given integer declaration,
/// populating \p region viewed as a DeclRegion with the results.
void DeclProducer::createImplicitDecls(IntegerDecl *intDecl)
{
    FunctionDecl *equals =
        createPredicate(EQ_pred, intDecl->getType(), intDecl);
    intDecl->addDecl(equals);
}

FunctionDecl *
DeclProducer::createPredicate(PredicateKind kind, Type *paramType,
                              DeclRegion *parent)
{
    IdentifierInfo *name = getPredicateName(kind);
    IdentifierInfo *paramX = resource->getIdentifierInfo("X");
    IdentifierInfo *paramY = resource->getIdentifierInfo("Y");

    ParamValueDecl *params[] = {
        new ParamValueDecl(paramX, paramType, PM::MODE_DEFAULT, 0),
        new ParamValueDecl(paramY, paramType, PM::MODE_DEFAULT, 0)
    };

    FunctionDecl *pred =
        new FunctionDecl(name, 0, params, 2, theBoolDecl->getType(),
                         parent);
    pred->setAsPrimitive(getPredicatePrimitive(kind));
    return pred;
}
