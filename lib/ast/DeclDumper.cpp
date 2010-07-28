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

void DeclDumper::visitPackageDecl(PackageDecl *node)
{
    printHeader(node);
    indent();

    typedef DeclRegion::DeclIter iterator;

    iterator I = node->beginDecls();
    iterator E = node->endDecls();
    for ( ; I != E; ++I) {
        S << '\n';
        printIndentation();
        visitDecl(*I);
    }

    if (node->hasPrivatePart()) {
        S << "\n\n";
        printIndentation();
        S << "<private";
        indent();

        I = node->getPrivatePart()->beginDecls();
        E = node->getPrivatePart()->endDecls();
        for ( ; I != E; ++I) {
            S << '\n';
            printIndentation();
            visitDecl(*I);
        }
        S << '>';
        dedent();
    }

    if (node->hasImplementation()) {
        S << "\n\n";
        printIndentation();
        S << "<body";
        indent();

        I = node->getImplementation()->beginDecls();
        E = node->getImplementation()->endDecls();
        for ( ; I != E; ++I) {
            S << '\n';
            printIndentation();
            visitDecl(*I);
        }
        S << '>';
        dedent();
    }

    S << '>';
}

void DeclDumper::visitUseDecl(UseDecl *node)
{
    printHeader(node) << '>';
}

void DeclDumper::visitBodyDecl(BodyDecl *node)
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

void DeclDumper::visitPrivateTypeDecl(PrivateTypeDecl *node)
{
    unsigned tags = node->getTypeTags();
    printHeader(node);

    if (tags & PrivateTypeDecl::Abstract)
        S << " abstract";
    if (tags & PrivateTypeDecl::Tagged)
        S << " tagged";
    if (tags & PrivateTypeDecl::Limited)
        S << " limited";
    S << '>';
}

void DeclDumper::visitAccessDecl(AccessDecl *node)
{
    printHeader(node) << ' ';
    dumpAST(node->getType());
    S << '>';
}

void DeclDumper::visitRecordDecl(RecordDecl *node)
{
    printHeader(node);
    indent();

    typedef DeclRegion::DeclIter iterator;
    iterator I = node->beginDecls();
    iterator E = node->endDecls();
    for ( ; I != E; ++I) {
        S << '\n';
        printIndentation();
        visitDecl(*I);
    }
    dedent();
    S << '>';
}

void DeclDumper::visitComponentDecl(ComponentDecl *node)
{
    printHeader(node) << ' ';
    dumpAST(node->getType());
    S << '>';
}
