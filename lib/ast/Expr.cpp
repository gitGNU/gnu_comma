//===-- ast/Expr.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/Qualifier.h"

#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


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

FunctionCallExpr::FunctionCallExpr(FunctionDecl *connective,
                                   Expr **args,
                                   unsigned numArgs,
                                   Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      connective(connective),
      numArgs(numArgs),
      qualifier(0)
{
    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(FunctionDecl **connectives,
                                   unsigned numConnectives,
                                   Expr **args,
                                   unsigned numArgs,
                                   Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      numArgs(numArgs),
      qualifier(0)
{
    if (numConnectives > 1)
        connective = new OverloadedDeclName(connectives,
                                            connectives + numConnectives);
    else {
        connective = connectives[0];
        setTypeForConnective();
    }

    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

void FunctionCallExpr::setTypeForConnective()
{
    FunctionDecl *fdecl = cast<FunctionDecl>(connective);
    setType(fdecl->getReturnType());
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

FunctionDecl *FunctionCallExpr::getConnective(unsigned i) const
{
    assert(i < numConnectives() && "Connective index out of range!");

    if (OverloadedDeclName *odn = dyn_cast<OverloadedDeclName>(connective))
        return cast<FunctionDecl>(odn->getOverload(i));
    else
        return cast<FunctionDecl>(connective);
}

void FunctionCallExpr::resolveConnective(FunctionDecl *decl)
{
    OverloadedDeclName *odn = cast<OverloadedDeclName>(connective);

    connective = decl;
    setType(decl->getReturnType());
    delete odn;
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

//===----------------------------------------------------------------------===//
// IndexedArrayExpr

IndexedArrayExpr::IndexedArrayExpr(DeclRefExpr *arrExpr,
                                   Expr **indices, unsigned numIndices)
    : Expr(AST_IndexedArrayExpr, arrExpr->getLocation()),
      indexedArray(arrExpr),
      numIndices(numIndices)
{
    assert(numIndices != 0 && "Missing indices!");

    TypedefType *defTy = cast<TypedefType>(arrExpr->getType());
    ArrayType *arrTy = cast<ArrayType>(defTy->getBaseType());
    setType(arrTy->getComponentType());

    indexExprs = new Expr*[numIndices];
    std::copy(indices, indices + numIndices, indexExprs);
}
