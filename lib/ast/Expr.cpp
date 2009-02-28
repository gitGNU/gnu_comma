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
                                   Expr        **args,
                                   unsigned      numArgs,
                                   Location      loc)
    : Expr(AST_FunctionCallExpr, connective->getReturnType(), loc),
      connective(connective),
      numArgs(numArgs),
      qualifier(0)
{
    arguments = new Expr*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

FunctionCallExpr::~FunctionCallExpr()
{
    delete[] arguments;
    if (isAmbiguous()) delete getOptions();
    if (isQualified()) delete qualifier;
}

void FunctionCallExpr::addConnective(FunctionDecl *connective)
{
    if (isAmbiguous()) {
        OptionVector *options = getOptions();
        options->push_back(connective);
    }
    else {
        FunctionDecl *original = this->connective;
        connectiveOptions = new OptionVector();
        connectiveOptions->push_back(original);
        connectiveOptions->push_back(connective);
        connectiveBits |= AMBIGUOUS_BIT;

        // Set the underlying type of this expression to NULL, blocking access
        // to the type until this node has been resolved.
        setType(0);
    }
}

unsigned FunctionCallExpr::numConnectives() const
{
    if (isAmbiguous())
        return getOptions()->size();
    else
        return 1;
}

FunctionDecl *FunctionCallExpr::getConnective(unsigned i) const
{
    assert(i < numConnectives() && "Connective index out of range!");

    if (isUnambiguous())
        return connective;
    else {
        OptionVector *options = getOptions();
        return options->operator[](i);
    }
}
