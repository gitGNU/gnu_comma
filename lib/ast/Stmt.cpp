//===-- ast/Stmt.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/DSTDefinition.h"
#include "comma/ast/ExceptionRef.h"
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
// HandlerStmt
HandlerStmt::HandlerStmt(Location loc, ExceptionRef **refs, unsigned numRefs)
    : StmtSequence(AST_HandlerStmt),
      loc(loc), numChoices(numRefs)
{
    choices = new ExceptionRef*[numChoices];
    std::memcpy(choices, refs, sizeof(ExceptionRef*)*numRefs);
}

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

ForStmt::ForStmt(Location loc, LoopDecl *iterationDecl, DSTDefinition *control)
    : Stmt(AST_ForStmt),
      location(loc),
      iterationDecl(iterationDecl),
      control(control)
{
    assert(control->getTag() != DSTDefinition::Unconstrained_DST &&
           "Invalid discrete subtype definition for loop control!");
    assert(control->getType() == iterationDecl->getType() &&
           "Inconsistent types!");
}

//===----------------------------------------------------------------------===//
// RaiseStmt

const ExceptionDecl *RaiseStmt::getExceptionDecl() const
{
    return ref->getException();
}

ExceptionDecl *RaiseStmt::getExceptionDecl()
{
    return ref->getException();
}
