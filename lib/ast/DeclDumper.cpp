//===-- ast/DeclDumper.cpp ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "DeclDumper.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"

#include "llvm/Support/Format.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

llvm::raw_ostream &DeclDumper::dump(Decl *decl, unsigned level)
{
    unsigned savedLevel = indentLevel;
    indentLevel = level;
    visitDecl(decl);
    indentLevel = savedLevel;
    return S;
}

llvm::raw_ostream &DeclDumper::printHeader(Ast *node)
{
    const char *nameString = cast<Decl>(node)->getString();;
    AstDumperBase::printHeader(node) << llvm::format(" '%s'", nameString);
    return S;
}

//===----------------------------------------------------------------------===//
// Visitor implementations.

void DeclDumper::visitUseDecl(UseDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitAddDecl(AddDecl *node)
{
    printHeader(node);

    if (node->countDecls()) {
        indent();
        DeclRegion::DeclRegion::DeclIter I = node->beginDecls();
        DeclRegion::DeclRegion::DeclIter E = node->endDecls();
        for ( ; I != E; ++I) {
            S << '\n';
            printIndentation();
            visitDecl(*I);
        }
        dedent();
    }
    S << '>';
}

void DeclDumper::visitSubroutineDecl(SubroutineDecl *node)
{
    printHeader(node) << '\n';
    indent();
    printIndentation();
    dumpAST(node->getType());
    if (!node->isForwardDeclaration() && node->hasBody()) {
        S << '\n';
        printIndentation();
        dumpAST(node->getBody());
    }
    dedent();
    S << '>';
}

void DeclDumper::visitFunctionDecl(FunctionDecl *node)
{
    visitSubroutineDecl(node);
}

void DeclDumper::visitProcedureDecl(ProcedureDecl *node)
{
    visitSubroutineDecl(node);
}

void DeclDumper::visitParamValueDecl(ParamValueDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitLoopDecl(LoopDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitObjectDecl(ObjectDecl *node)
{
    printHeader(node);
    if (node->hasInitializer()) {
        S << '\n';
        indent();
        printIndentation();
        dumpAST(node->getInitializer());
        dedent();
    }
    S << '>';
}

void DeclDumper::visitEnumLiteral(EnumLiteral *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitEnumerationDecl(EnumerationDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitIntegerDecl(IntegerDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitArrayDecl(ArrayDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitExceptionDecl(ExceptionDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitIncompleteTypeDecl(IncompleteTypeDecl *node)
{
    printHeader(node) << '>';
}

