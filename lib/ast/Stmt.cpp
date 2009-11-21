//===-- ast/Stmt.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"

#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// ProcedureCallStmt
ProcedureCallStmt::ProcedureCallStmt(SubroutineRef *ref,
                                     Expr **posArgs, unsigned numPos,
                                     KeywordSelector **keys, unsigned numKeys)
    : Stmt(AST_ProcedureCallStmt),
      SubroutineCall(ref, posArgs, numPos, keys, numKeys),
      location(ref->getLocation())
{
    assert(ref->isResolved() && "Cannot form unresolved procedure calls!");
}

//===----------------------------------------------------------------------===//
// ReturnStmt
ReturnStmt::~ReturnStmt()
{
    if (returnExpr) delete returnExpr;
}

//===----------------------------------------------------------------------===//
// ForStmt

// The getControl methods are out of line to avoid inclusion of RangeAttrib.h.
const Ast *ForStmt::getControl() const
{
    return control;
}

Ast *ForStmt::getControl()
{
    return control;
}
