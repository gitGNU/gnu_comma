//===-- ast/ExprVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/ExprVisitor.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

/// Macro to help form switch dispatch tables.  Note that this macro will only
/// work with concrete nodes (Those with a definite kind).
#define DISPATCH(TYPE, NODE)         \
    Ast::AST_ ## TYPE:               \
    visit ## TYPE(cast<TYPE>(NODE)); \
    break

void ExprVisitor::visitAst(Ast *node)
{
    if (Expr *expr = dyn_cast<Expr>(node))
        visitExpr(expr);
}

void ExprVisitor::visitExpr(Expr *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of node!");
        break;
    case DISPATCH(DeclRefExpr, node);
    case DISPATCH(FunctionCallExpr, node);
    case DISPATCH(IndexedArrayExpr, node);
    case DISPATCH(InjExpr, node);
    case DISPATCH(PrjExpr, node);
    case DISPATCH(IntegerLiteral, node);
    };
}

//===----------------------------------------------------------------------===//
// Leaf nodes.  Default implementations do nothing.

void ExprVisitor::visitDeclRefExpr(DeclRefExpr *node) { }
void ExprVisitor::visitFunctionCallExpr(FunctionCallExpr *node) { }
void ExprVisitor::visitIndexedArrayExpr(IndexedArrayExpr *node) { }
void ExprVisitor::visitInjExpr(InjExpr *node) { }
void ExprVisitor::visitPrjExpr(PrjExpr *node) { }
void ExprVisitor::visitIntegerLiteral(IntegerLiteral *node) { }
