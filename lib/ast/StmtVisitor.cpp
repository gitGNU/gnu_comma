//===-- ast/StmtVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/StmtVisitor.h"
#include "comma/ast/Stmt.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

void StmtVisitor::visitAst(Ast *node)
{
    if (Stmt *stmt = dyn_cast<Stmt>(node))
        visitStmt(stmt);
}

/// Macro to help form switch dispatch tables.  Note that this macro will only
/// work with concrete nodes (Those with a definite kind).
#define DISPATCH(TYPE, NODE)         \
    Ast::AST_ ## TYPE:               \
    visit ## TYPE(cast<TYPE>(NODE)); \
    break

void StmtVisitor::visitStmt(Stmt *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;

    case DISPATCH(StmtSequence, node);
    case DISPATCH(BlockStmt, node);
    case DISPATCH(ProcedureCallStmt, node);
    case DISPATCH(ReturnStmt, node);
    case DISPATCH(AssignmentStmt, node);
    case DISPATCH(IfStmt, node);
    case DISPATCH(WhileStmt, node);
    }
}

void StmtVisitor::visitStmtSequence(StmtSequence *node)
{
    if (BlockStmt *block = dyn_cast<BlockStmt>(node))
        visitBlockStmt(block);
}

void StmtVisitor::visitBlockStmt(BlockStmt *node) { }
void StmtVisitor::visitProcedureCallStmt(ProcedureCallStmt *node) { }
void StmtVisitor::visitReturnStmt(ReturnStmt *node) { }
void StmtVisitor::visitAssignmentStmt(AssignmentStmt *node) { }
void StmtVisitor::visitIfStmt(IfStmt *node) { }
void StmtVisitor::visitWhileStmt(WhileStmt *node) { }
