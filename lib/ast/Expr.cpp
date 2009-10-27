//===-- ast/Expr.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"

#include <cstring>

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
      SubroutineCall(connective, posArgs, numPos, keyArgs, numKeys)
{
    setTypeForConnective();
}

void FunctionCallExpr::setTypeForConnective()
{
    if (isUnambiguous()) {
        FunctionDecl *fdecl = getConnective();
        setType(fdecl->getReturnType());
    }
}

void FunctionCallExpr::resolveConnective(FunctionDecl *decl)
{
    SubroutineCall::resolveConnective(decl);
    setTypeForConnective();
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

    ArrayType *arrTy = arrExpr->getType()->getAsArrayType();
    setType(arrTy->getComponentType());

    indexExprs = new Expr*[numIndices];
    std::copy(indices, indices + numIndices, indexExprs);
}

//===----------------------------------------------------------------------===//
// StringLiteral

void StringLiteral::init(const char *string, unsigned len)
{
    this->rep = new char[len];
    this->len = len;
    std::strncpy(this->rep, string, len);
}

StringLiteral::const_component_iterator
StringLiteral::findComponent(EnumerationType *type) const
{
    EnumerationType *root = type->getRootType();

    const_component_iterator I = begin_component_types();
    const_component_iterator E = end_component_types();
    for ( ; I != E; ++I) {
        const EnumerationDecl *decl = *I;
        if (root == decl->getType()->getRootType())
            return I;
    }
    return E;
}

StringLiteral::component_iterator
StringLiteral::findComponent(EnumerationType *type)
{
    EnumerationType *root = type->getRootType();

    component_iterator I = begin_component_types();
    component_iterator E = end_component_types();
    for ( ; I != E; ++I) {
        EnumerationDecl *decl = *I;
        if (root == decl->getType()->getRootType())
            return I;
    }
    return E;
}

bool StringLiteral::resolveComponentType(EnumerationType *type)
{
    component_iterator I = findComponent(type);

    if (I == end_component_types())
        return false;

    EnumerationDecl *decl = *I;
    interps.clear();
    interps.insert(decl);
    return true;
}
