//===-- ast/ExprDumper.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "ExprDumper.h"
#include "comma/ast/Expr.h"

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

void ExprDumper::visitPositionalAggExpr(PositionalAggExpr *node)
{
    typedef PositionalAggExpr::iterator iterator;
    iterator I = node->begin_components();
    iterator E = node->end_components();

    printHeader(node);
    indent();
    for ( ; I != E; ++I) {
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

