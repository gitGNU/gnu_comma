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
                                     Expr         **args,
                                     unsigned       numArgs,
                                     Location       loc)
    : Stmt(AST_ProcedureCallStmt),
      connective(connective),
      arguments(0),
      numArgs(numArgs),
      location(loc)
{
    if (numArgs) {
        arguments = new Expr*[numArgs];
        std::copy(args, args + numArgs, arguments);
    }
}

ProcedureCallStmt::~ProcedureCallStmt()
{
    if (arguments) delete[] arguments;
}

//===----------------------------------------------------------------------===//
// ReturnStmt
ReturnStmt::~ReturnStmt()
{
    if (returnExpr) delete returnExpr;
}

//===----------------------------------------------------------------------===//
// IfStmt
void IfStmt::dump(unsigned depth)
{
    dumpSpaces(depth++);
    std::cerr << '<' << getKindString()
              << ' ' << std::hex << uintptr_t(this) << '\n';

    condition->dump(depth);
    std::cerr << '\n';
    consequent->dump(depth);

    iterator endIter = endElsif();
    for (iterator iter = beginElsif(); iter != endIter; ++iter) {
        Elsif &elsif = *iter;
        std::cerr << '\n';
        dumpSpaces(depth);
        std::cerr << "elsif:\n";
        elsif.getCondition()->dump(depth + 1);

        std::cerr << '\n';
        elsif.getConsequent()->dump(depth + 1);
    }

    if (hasAlternate()) {
        std::cerr << '\n';
        alternate->dump(depth);
    }
    std::cerr << '>';
}
