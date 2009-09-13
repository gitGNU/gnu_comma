//===-- ast/ExprVisitor.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file provides a virtual class used for implementing the vistor
/// pattern across expression nodes.
///
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_EXPRVISITOR_HDR_GUARD
#define COMMA_AST_EXPRVISITOR_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

class ExprVisitor {

public:
    virtual ~ExprVisitor() { }

    virtual void visitAst(Ast *node);
    virtual void visitExpr(Expr *node);
    virtual void visitDeclRefExpr(DeclRefExpr *node);
    virtual void visitFunctionCallExpr(FunctionCallExpr *node);
    virtual void visitIndexedArrayExpr(IndexedArrayExpr *node);
    virtual void visitInjExpr(InjExpr *node);
    virtual void visitPrjExpr(PrjExpr *node);
    virtual void visitIntegerLiteral(IntegerLiteral *node);
};

} // end comma namespace.

#endif
