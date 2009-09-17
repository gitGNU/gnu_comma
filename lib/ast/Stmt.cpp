//===-- ast/Stmt.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Stmt.h"

#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// ProcedureCallStmt
ProcedureCallStmt::ProcedureCallStmt(
    SubroutineRef *ref,
    Expr **positionalArgs, unsigned numPositional,
    KeywordSelector **keys, unsigned numKeys)
    : Stmt(AST_ProcedureCallStmt),
      connective(cast<ProcedureDecl>(ref->getDeclaration(0))),
      location(ref->getLocation())
{
    assert(ref->isResolved() && "Cannot form unresolved procedure calls!");
    delete ref;

    setArguments(positionalArgs, numPositional, keys, numKeys);
}

ProcedureCallStmt::ProcedureCallStmt(ProcedureDecl *connective,
                                     Expr **posArgs,
                                     unsigned numPos,
                                     KeywordSelector **keys,
                                     unsigned numKeys,
                                     Location loc)
    : Stmt(AST_ProcedureCallStmt),
      connective(connective),
      location(loc)
{
    setArguments(posArgs, numPos, keys, numKeys);
}

void ProcedureCallStmt::setArguments(Expr **posArgs, unsigned numPos,
                                     KeywordSelector **keys, unsigned numKeys)
{
    this->numArgs = numPos + numKeys;
    this->numKeys = numKeys;

    if (numArgs) {
        arguments = new Expr*[numArgs];
        std::copy(posArgs, posArgs + numPos, arguments);
        std::fill(arguments + numPos, arguments + numArgs, (Expr*)0);
    }
    else
        arguments = 0;

    if (numKeys) {
        keyedArgs = new KeywordSelector*[numKeys];
        std::copy(keys, keys + numKeys, keyedArgs);
    }
    else
        keyedArgs = 0;

    for (unsigned i = 0; i < numKeys; ++i) {
        KeywordSelector *selector = keyedArgs[i];
        IdentifierInfo *key = selector->getKeyword();
        Expr *expr = selector->getExpression();
        int indexResult = connective->getKeywordIndex(key);

        assert(indexResult >= 0 && "Could not resolve keyword index!");

        unsigned argIndex = unsigned(indexResult);
        assert(argIndex >= numPos && "Keyword resolved to a positional index!");
        assert(argIndex < getNumArgs() && "Keyword index too large!");
        assert(arguments[argIndex] == 0 && "Duplicate keywords!");

        arguments[argIndex] = expr;
    }
}

ProcedureCallStmt::~ProcedureCallStmt()
{
    delete[] arguments;
    delete[] keyedArgs;
}

//===----------------------------------------------------------------------===//
// ReturnStmt
ReturnStmt::~ReturnStmt()
{
    if (returnExpr) delete returnExpr;
}
