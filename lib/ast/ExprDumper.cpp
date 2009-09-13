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

void ExprDumper::visitKeywordSelector(KeywordSelector *node)
{
    printHeader(node)
        << llvm::format(" '%s'\n", node->getKeyword()->getString());
    indent();
    printIndentation();
    visitExpr(node->getExpression());
    dedent();
    S << '>';
}

void ExprDumper::visitFunctionCallExpr(FunctionCallExpr *node)
{
    printHeader(node);

    SubroutineRef *connective = node->getConnective();
    S << llvm::format(" '%s'", connective->getString());

    unsigned numArgs = node->getNumArgs();
    unsigned index = 0;
    indent();
    while (index < numArgs) {
        S << '\n';
        printIndentation();
        visitExpr(node->getArg(index));
        if (++index < numArgs)
            S << "; ";
    }
    dedent();
    S << '>';
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

