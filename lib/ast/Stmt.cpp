//===-- ast/Stmt.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/DSTDefinition.h"
#include "comma/ast/ExceptionRef.h"
#include "comma/ast/Expr.h"
#include "comma/ast/KeywordSelector.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"

#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Stmt
bool Stmt::isTerminator() const
{
    if (isa<ReturnStmt>(this) || isa<RaiseStmt>(this))
        return true;

    // Exit statements without a condition are considered terminators.
    if (const ExitStmt *exit = dyn_cast<ExitStmt>(this))
        return !exit->hasCondition();

    return false;
}

//===----------------------------------------------------------------------===//
// StmtSequence
bool StmtSequence::hasCatchAll() const
{
    if (isHandled())
        return handlers.back()->isCatchAll();
    return false;
}

bool StmtSequence::handles(const ExceptionDecl *exception) const
{
    if (!isHandled())
        return false;

    if (hasCatchAll())
        return true;

    for (const_handler_iter I = handler_begin(); I != handler_end(); ++I)
        if ((*I)->handles(exception))
            return true;

    return false;
}

//===----------------------------------------------------------------------===//
// HandlerStmt
HandlerStmt::HandlerStmt(Location loc, ExceptionRef **refs, unsigned numRefs)
    : StmtSequence(AST_HandlerStmt, loc),
      numChoices(numRefs)
{
    choices = new ExceptionRef*[numChoices];
    std::memcpy(choices, refs, sizeof(ExceptionRef*)*numRefs);
}

bool HandlerStmt::handles(const ExceptionDecl *exception) const
{
    for (const_choice_iterator I = choice_begin(); I != choice_end(); ++I) {
        const ExceptionRef *ref = *I;
        if (ref->getException() == exception)
            return true;
    }
    return false;
}

//===----------------------------------------------------------------------===//
// ProcedureCallStmt
ProcedureCallStmt::ProcedureCallStmt(SubroutineRef *ref,
                                     Expr **posArgs, unsigned numPos,
                                     KeywordSelector **keys, unsigned numKeys)
    : Stmt(AST_ProcedureCallStmt, ref->getLocation()),
      SubroutineCall(ref, posArgs, numPos, keys, numKeys)
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
// AssignmentStmt
AssignmentStmt::AssignmentStmt(Expr *target, Expr *value)
    : Stmt(AST_AssignmentStmt, target->getLocation()),
      target(target), value(value) { }

//===----------------------------------------------------------------------===//
// ForStmt
ForStmt::ForStmt(Location loc, LoopDecl *iterationDecl, DSTDefinition *control)
    : IterationStmt(AST_ForStmt, loc),
      iterationDecl(iterationDecl),
      control(control)
{
    assert(control->getTag() != DSTDefinition::Unconstrained_DST &&
           "Invalid discrete subtype definition for loop control!");
    assert(control->getType() == iterationDecl->getType() &&
           "Inconsistent types!");
}

//===----------------------------------------------------------------------===//
// PragmaStmt
PragmaStmt::PragmaStmt(Pragma *pragma)
    : Stmt(AST_PragmaStmt, pragma->getLocation()), pragma(pragma)
{ }

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
