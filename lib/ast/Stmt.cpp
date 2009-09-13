//===-- ast/Stmt.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include <iostream>

using namespace comma;

//===----------------------------------------------------------------------===//
// ProcedureCallStmt
ProcedureCallStmt::ProcedureCallStmt(ProcedureDecl *connective,
                                     Expr **posArgs,
                                     unsigned numPos,
                                     KeywordSelector **keys,
                                     unsigned numKeys,
                                     Location loc)
    : Stmt(AST_ProcedureCallStmt),
      connective(connective),
      arguments(0),
      keyedArgs(0),
      numArgs(numPos + numKeys),
      numKeys(numKeys),
      location(loc)
{
    if (numArgs) {
        arguments = new Expr*[numArgs];
        std::copy(posArgs, posArgs + numPos, arguments);
        std::fill(arguments + numPos, arguments + numArgs, (Expr*)0);
    }

    if (numKeys) {
        keyedArgs = new KeywordSelector*[numKeys];
        std::copy(keys, keys + numKeys, keyedArgs);
    }

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
