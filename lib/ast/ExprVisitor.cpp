//===-- ast/ExprVisitor.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AttribExpr.h"
#include "comma/ast/AggExpr.h"
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
    if (isa<AttribExpr>(node))
        visitAttribExpr(cast<AttribExpr>(node));
    else {
        switch (node->getKind()) {
        default:
            assert(false && "Cannot visit this kind of node!");
            break;
        case DISPATCH(DeclRefExpr, node);
        case DISPATCH(FunctionCallExpr, node);
        case DISPATCH(IndexedArrayExpr, node);
        case DISPATCH(SelectedExpr, node);
        case DISPATCH(IntegerLiteral, node);
        case DISPATCH(StringLiteral, node);
        case DISPATCH(AggregateExpr, node);
        case DISPATCH(ConversionExpr, node);
        case DISPATCH(NullExpr, node);
        case DISPATCH(QualifiedExpr, node);
        case DISPATCH(DereferenceExpr, node);
        case DISPATCH(AllocatorExpr, node);
        case DISPATCH(DiamondExpr, node);
        };
    }
}

void ExprVisitor::visitAttribExpr(AttribExpr *node)
{
    switch (node->getKind()) {
    default:
        assert(false && "Cannot visit this kind of attribute!");
        break;
    case DISPATCH(FirstAE, node);
    case DISPATCH(FirstArrayAE, node);
    case DISPATCH(LastArrayAE, node);
    case DISPATCH(LengthAE, node);
    case DISPATCH(LastAE, node);
    };
}

//===----------------------------------------------------------------------===//
// Leaf nodes.  Default implementations do nothing.

void ExprVisitor::visitDeclRefExpr(DeclRefExpr *node) { }
void ExprVisitor::visitFunctionCallExpr(FunctionCallExpr *node) { }
void ExprVisitor::visitIndexedArrayExpr(IndexedArrayExpr *node) { }
void ExprVisitor::visitSelectedExpr(SelectedExpr *node) { }
void ExprVisitor::visitIntegerLiteral(IntegerLiteral *node) { }
void ExprVisitor::visitStringLiteral(StringLiteral *node) { }
void ExprVisitor::visitAggregateExpr(AggregateExpr *node) { }
void ExprVisitor::visitConversionExpr(ConversionExpr *node) { }
void ExprVisitor::visitNullExpr(NullExpr *node) { }
void ExprVisitor::visitQualifiedExpr(QualifiedExpr *node) { }
void ExprVisitor::visitDereferenceExpr(DereferenceExpr *node) { }
void ExprVisitor::visitAllocatorExpr(AllocatorExpr *node) { }
void ExprVisitor::visitDiamondExpr(DiamondExpr *node) { }

void ExprVisitor::visitFirstAE(FirstAE *node) { }
void ExprVisitor::visitFirstArrayAE(FirstArrayAE *node) { }
void ExprVisitor::visitLastArrayAE(LastArrayAE *node) { }
void ExprVisitor::visitLengthAE(LengthAE *node) { }
void ExprVisitor::visitLastAE(LastAE *node) { }
