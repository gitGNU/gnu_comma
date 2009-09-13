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

FunctionCallExpr::FunctionCallExpr(SubroutineRef *connective,
                                   Expr **args, unsigned numArgs)
    : Expr(AST_FunctionCallExpr, connective->getLocation()),
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
                                   Expr **args, unsigned numArgs,
                                   Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      connective(0),
      numArgs(numArgs),
      qualifier(0)
{
    connective = new SubroutineRef(loc, connectives,
                                   connectives + numConnectives);
    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(FunctionDecl *fdecl,
                                   Expr **args, unsigned numArgs,
                                   Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      connective(new SubroutineRef(loc, fdecl)),
      numArgs(numArgs),
      qualifier(0)
{
    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);
    setType(fdecl->getReturnType());
}

void FunctionCallExpr::setTypeForConnective()
{
    if (numConnectives() == 1) {
        FunctionDecl *fdecl = getConnective(0);
        setType(fdecl->getReturnType());
    }
}

FunctionCallExpr::~FunctionCallExpr()
{
    delete[] arguments;
    delete connective;
}

void FunctionCallExpr::resolveConnective(FunctionDecl *decl)
{
    connective->resolve(decl);
    setType(decl->getReturnType());
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
