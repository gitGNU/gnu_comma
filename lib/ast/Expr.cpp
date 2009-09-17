//===-- ast/Expr.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Qualifier.h"

#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


//===----------------------------------------------------------------------===//
// FunctionCallExpr

FunctionCallExpr::FunctionCallExpr(SubroutineRef *connective,
                                   Expr **posArgs, unsigned numPos,
                                   KeywordSelector **keyArgs, unsigned numKeys)
    : Expr(AST_FunctionCallExpr, connective->getLocation()),
      connective(connective),
      qualifier(0)
{
    setArguments(posArgs, numPos, keyArgs, numKeys);
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(FunctionDecl **connectives,
                                   unsigned numConnectives,
                                   Expr **posArgs, unsigned numPos,
                                   KeywordSelector **keyArgs, unsigned numKeys,
                                   Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      qualifier(0)
{
    connective = new SubroutineRef(loc, connectives,
                                   connectives + numConnectives);

    setArguments(posArgs, numPos, keyArgs, numKeys);
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(FunctionDecl *fdecl,
                                   Expr **posArgs, unsigned numPos,
                                   KeywordSelector **keyArgs, unsigned numKeys,
                                   Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      connective(new SubroutineRef(loc, fdecl)),
      qualifier(0)
{
    setArguments(posArgs, numPos, keyArgs, numKeys);
    setTypeForConnective();
}

void FunctionCallExpr::setArguments(Expr **posArgs, unsigned numPos,
                                    KeywordSelector **keyArgs, unsigned numKeys)
{
    this->numPositional = numPos;
    this->numKeys = numKeys;

    unsigned numArgs = numPos + numKeys;

    if (numArgs) {
        arguments = new Expr*[numArgs];
        std::copy(posArgs, posArgs + numPos, arguments);
        std::fill(arguments + numPos, arguments + numArgs, (Expr*)0);
    }
    else
        arguments = 0;

    if (numKeys) {
        keyedArgs = new KeywordSelector*[numKeys];
        std::copy(keyArgs, keyArgs + numKeys, keyedArgs);
    }
    else
        keyedArgs = 0;
}

void FunctionCallExpr::setTypeForConnective()
{
    if (numConnectives() == 1) {
        FunctionDecl *fdecl = getConnective(0);
        resolveConnective(fdecl);
    }
}

FunctionCallExpr::~FunctionCallExpr()
{
    delete connective;
    delete[] arguments;
    delete[] keyedArgs;
}

void FunctionCallExpr::resolveConnective(FunctionDecl *decl)
{
    connective->resolve(decl);
    setType(decl->getReturnType());

    assert(decl->getArity() == getNumArgs() && "Arity mismatch!");

    // Fill in the argument vector with any keyed expressions, sorted so that
    // they match what the given function decl requires.
    for (unsigned i = 0; i < numKeys; ++i) {
        KeywordSelector *selector = keyedArgs[i];
        IdentifierInfo *key = selector->getKeyword();
        Expr *expr = selector->getExpression();
        int indexResult = decl->getKeywordIndex(key);

        assert(indexResult >= 0 && "Could not resolve keyword index!");

        unsigned argIndex = unsigned(indexResult);
        assert(argIndex >= numPositional &&
               "Keyword resolved to a positional index!");
        assert(argIndex < getNumArgs() && "Keyword index too large!");
        assert(arguments[argIndex] == 0 && "Duplicate keywords!");

        arguments[argIndex] = expr;
    }
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
