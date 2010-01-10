//===-- ast/ExprDumper.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "ExprDumper.h"
#include "comma/ast/AggExpr.h"

#include "llvm/Support/Format.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

llvm::raw_ostream &ExprDumper::dump(Expr *expr, unsigned level)
{
    unsigned savedLevel = indentLevel;
    indentLevel = level;
    visitExpr(expr);
    indentLevel = savedLevel;
    return S;
}

void ExprDumper::visitDeclRefExpr(DeclRefExpr *node)
{
    printHeader(node) <<
        llvm::format(" '%s'>", node->getString());
}

void ExprDumper::visitFunctionCallExpr(FunctionCallExpr *node)
{
    printHeader(node);
    FunctionDecl *decl = node->getConnective(0);
    S << llvm::format(" '%s'>", decl->getString());
}

void ExprDumper::visitIndexedArrayExpr(IndexedArrayExpr *node)
{
    printHeader(node) << '>';
}

void ExprDumper::visitInjExpr(InjExpr *node)
{
    printHeader(node) << '>';
}

void ExprDumper::visitPrjExpr(PrjExpr *node)
{
    printHeader(node) << '>';
}

void ExprDumper::visitIntegerLiteral(IntegerLiteral *node)
{
    printHeader(node) << " '";
    node->getValue().print(S, false);
    S << "'>";
}

void ExprDumper::visitStringLiteral(StringLiteral *node)
{
    printHeader(node) << " \"" << node->getString() << "\">";
}

void ExprDumper::visitAggregateExpr(AggregateExpr *node)
{
    printHeader(node);
    indent();

    typedef AggregateExpr::pos_iterator pos_iterator;
    for (pos_iterator I = node->pos_begin(), E = node->pos_end(); I != E; ++I) {
        S << '\n';
        dump(*I, indentLevel);
    }
    dedent();
    S << ">";
}

void ExprDumper::visitConversionExpr(ConversionExpr *node)
{
    printHeader(node) << " ";
    visitExpr(node->getOperand());
    S << " ";
    dumpAST(node->getType()) << ">";
}

void ExprDumper::visitNullExpr(NullExpr *node)
{
    printHeader(node) << '>';
}

void ExprDumper::visitQualifiedExpr(QualifiedExpr *node)
{
    printHeader(node) << ' ';
    dumpAST(node->getPrefix()) << ' ';
    visitExpr(node->getOperand());
    S << '>';
}

