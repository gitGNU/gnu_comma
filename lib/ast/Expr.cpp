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

FunctionCallExpr::FunctionCallExpr(SubroutineRef *connective)
    : Expr(AST_FunctionCallExpr, connective->getLocation()),
      SubroutineCall(connective, 0, 0, 0, 0)
{
    setTypeForConnective();
}

FunctionCallExpr::FunctionCallExpr(FunctionDecl *connective, Location loc)
    : Expr(AST_FunctionCallExpr, loc),
      SubroutineCall(connective, 0, 0, 0, 0)
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

IndexedArrayExpr::IndexedArrayExpr(Expr *arrExpr,
                                   Expr **indices, unsigned numIndices)
    : Expr(AST_IndexedArrayExpr, arrExpr->getLocation()),
      indexedArray(arrExpr),
      numIndices(numIndices)
{
    assert(numIndices != 0 && "Missing indices!");

    if (arrExpr->hasType()) {
        ArrayType *arrTy = cast<ArrayType>(arrExpr->getType());
        setType(arrTy->getComponentType());
    }

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

//===----------------------------------------------------------------------===//
// KeyedAggExpr

KeyedAggExpr::ChoiceList::ChoiceList(Ast **choices, unsigned choiceEntries,
                                     Expr *expr)
  : choiceData(reinterpret_cast<Ast**>(this + 1)),
    choiceEntries(choiceEntries),
    expr(expr)
{
    std::copy(choices, choices + choiceEntries, choiceData);
}

KeyedAggExpr::ChoiceList *
KeyedAggExpr::ChoiceList::create(Ast **choices, unsigned numChoices, Expr *expr)
{
    assert(numChoices != 0 && "At leaast one choice must be present!");

    // Calculate the size of the needed ChoiceList and allocate the raw memory.
    unsigned size = sizeof(ChoiceList) + sizeof(Ast*) * numChoices;
    char *raw = new char[size];

    // Placement operator new using the classes constructor initializes the
    // internal structure.
    return new (raw) ChoiceList(choices, numChoices, expr);
}

void KeyedAggExpr::ChoiceList::dispose(ChoiceList *CL)
{
    // Deallocate the associated AST nodes.
    delete CL->expr;
    for (iterator I = CL->begin(); I != CL->end(); ++I)
        delete *I;

    // Cast CL back to a raw pointer to char and deallocate.
    char *raw = reinterpret_cast<char *>(CL);
    delete [] raw;
}

KeyedAggExpr::~KeyedAggExpr()
{
    for (cl_iterator I = cl_begin(); I != cl_end(); ++I)
        ChoiceList::dispose(*I);
}

unsigned KeyedAggExpr::numChoices() const
{
    unsigned result = 0;
    for (const_cl_iterator I = cl_begin(); I != cl_end(); ++I)
        result += (*I)->numChoices();
    return result;
}


