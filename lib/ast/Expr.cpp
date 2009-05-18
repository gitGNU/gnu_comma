//===-- ast/Expr.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Qualifier

// Returns the base type of this qualifier as a declarative region.
DeclRegion  *Qualifier::resolve()
{
    DeclRegion *region   = 0;
    QualPair    pair     = getBaseQualifier();
    Type       *baseType = pair.first;

    if (DomainType *domain = dyn_cast<DomainType>(baseType))
        region = domain->getDomoidDecl();
    else if (EnumerationType *etype = dyn_cast<EnumerationType>(baseType))
        region = etype->getDeclaration();
    else {
        CarrierType *carrier = cast<CarrierType>(baseType);
        baseType = carrier->getRepresentationType();
        region   = cast<DomainType>(baseType)->getDomainDecl();
    }

    assert(region && "Qualifier not a domain?");
    return region;
}

//===----------------------------------------------------------------------===//
// KeywordSelector

KeywordSelector::KeywordSelector(IdentifierInfo *key, Location loc, Expr *expr)
    : Expr(AST_KeywordSelector, loc),
      keyword(key),
      expression(expr)
{
    if (expression->hasType())
        this->setType(expression->getType());
}

//===----------------------------------------------------------------------===//
// FunctionCallExpr

FunctionCallExpr::FunctionCallExpr(Decl      *connective,
                                   Expr     **args,
                                   unsigned   numArgs,
                                   Location   loc)
    : Expr(AST_FunctionCallExpr, loc),
      connective(connective),
      numArgs(numArgs),
      qualifier(0)
{
    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(Decl     **connectives,
                                   unsigned   numConnectives,
                                   Expr     **args,
                                   unsigned   numArgs,
                                   Location   loc)
    : Expr(AST_FunctionCallExpr, loc),
      numArgs(numArgs),
      qualifier(0)
{
    if (numConnectives > 1)
        connective = new OverloadedDeclName(connectives, numConnectives);
    else {
        connective = connectives[0];
        setTypeForConnective();
    }

    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);

    // FIXME:  We should probably conditionalize over a better define.
#ifndef NDEBUG
    for (unsigned i = 0; i < numConnectives; ++i)
        assert((isa<FunctionDecl>(connectives[i]) ||
                isa<EnumLiteral>(connectives[i])) &&
               "Bad type for function call connective!");
#endif
}

void FunctionCallExpr::setTypeForConnective()
{
    if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(connective))
        setType(fdecl->getReturnType());
    else if (EnumLiteral *elit = dyn_cast<EnumLiteral>(connective))
        setType(elit->getType());
    else
        assert(false && "Bad type for function call connective!");
}

FunctionCallExpr::~FunctionCallExpr()
{
    delete[] arguments;

    if (OverloadedDeclName *odn = dyn_cast<OverloadedDeclName>(connective))
        delete odn;
}

unsigned FunctionCallExpr::numConnectives() const
{
    if (OverloadedDeclName *odn = dyn_cast<OverloadedDeclName>(connective))
        return odn->numOverloads();
    else
        return 1;
}

Decl *FunctionCallExpr::getConnective(unsigned i) const
{
    assert(i < numConnectives() && "Connective index out of range!");

    if (OverloadedDeclName *odn = dyn_cast<OverloadedDeclName>(connective))
        return odn->getOverload(i);
    else
        return cast<Decl>(connective);
}

void FunctionCallExpr::resolveConnective(FunctionDecl *decl)
{
    OverloadedDeclName *odn = dyn_cast<OverloadedDeclName>(connective);
    assert(odn && "Cannot resolve non-overloaded function calls!");

    connective = decl;
    setType(decl->getReturnType());
    delete odn;
}

void FunctionCallExpr::resolveConnective(EnumLiteral *decl)
{
    OverloadedDeclName *odn = dyn_cast<OverloadedDeclName>(connective);
    assert(odn && "Cannot resolove non-overloaded function calls!");
    assert(getNumArgs() == 0 &&
           "Enumeration calls cannot be applied to arguments!");

    connective = decl;
    setType(decl->getType());
    delete odn;
}

void FunctionCallExpr::resolveConnective(Decl *connective)
{
    if (FunctionDecl *fdecl = dyn_cast<FunctionDecl>(connective))
        resolveConnective(fdecl);
    else
        resolveConnective(cast<EnumLiteral>(connective));
}

bool FunctionCallExpr::containsConnective(FunctionType *ftype) const
{
    for (unsigned i = 0; i < numConnectives(); ++i) {
        FunctionDecl *connective = dyn_cast<FunctionDecl>(getConnective(i));
        if (connective) {
            FunctionType *connectiveType = connective->getType();
            if (connectiveType->equals(ftype))
                return true;
        }
    }
    return false;
}
