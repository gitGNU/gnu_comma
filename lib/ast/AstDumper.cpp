//===-- ast/AstDumper.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "AstDumper.h"
#include "ExprDumper.h"
#include "DeclDumper.h"
#include "StmtDumper.h"
#include "TypeDumper.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/RangeAttrib.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"

#include "llvm/Support/Format.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// AstDumperBase methods.

llvm::raw_ostream &AstDumperBase::printHeader(Ast *node)
{
    const char *kindString = node->getKindString();
    S << llvm::format("<%s ", kindString);
    S.write_hex(uintptr_t(node));
    return S;
}

llvm::raw_ostream &AstDumperBase::printIndentation()
{
    for (unsigned i = 0; i < indentLevel; ++i)
        S << ' ';
    return S;
}

llvm::raw_ostream &AstDumperBase::dumpParamMode(PM::ParameterMode mode)
{
    switch (mode) {
    case PM::MODE_DEFAULT:
        S << "D";
        break;
    case PM::MODE_IN:
        S << "I";
        break;
    case PM::MODE_OUT:
        S << "O";
        break;
    case PM::MODE_IN_OUT:
        S << "IO";
        break;
    }
    return S;
}

//===----------------------------------------------------------------------===//
// AstDumper methods.

AstDumper::AstDumper(llvm::raw_ostream &stream)
    : AstDumperBase(stream)
{
    EDumper = new ExprDumper(stream, this);
    DDumper = new DeclDumper(stream, this);
    SDumper = new StmtDumper(stream, this);
    TDumper = new TypeDumper(stream, this);
}

AstDumper::~AstDumper()
{
    delete EDumper;
    delete DDumper;
    delete SDumper;
    delete TDumper;
}

llvm::raw_ostream &AstDumper::dumpDecl(Decl *node)
{
    return DDumper->dump(node, indentLevel);
}

llvm::raw_ostream &AstDumper::dumpExpr(Expr *node)
{
    return EDumper->dump(node, indentLevel);
}

llvm::raw_ostream &AstDumper::dumpStmt(Stmt *node)
{
    return SDumper->dump(node, indentLevel);
}

llvm::raw_ostream &AstDumper::dumpType(Type *node)
{
    return TDumper->dump(node, indentLevel);
}

llvm::raw_ostream &AstDumper::dumpRangeAttrib(RangeAttrib *node)
{
    printHeader(node) << ' ';
    return dump(node->getPrefix(), indentLevel) << '>';
}

llvm::raw_ostream &AstDumper::dump(Ast *node, unsigned level)
{
    unsigned savedLevel = indentLevel;
    indentLevel = level;
    if (Decl *decl = dyn_cast<Decl>(node))
        dumpDecl(decl);
    else if (Stmt *stmt = dyn_cast<Stmt>(node))
        dumpStmt(stmt);
    else if (Expr *expr = dyn_cast<Expr>(node))
        dumpExpr(expr);
    else if (Type *type = dyn_cast<Type>(node))
        dumpType(type);
    else {
        switch (node->getKind()) {

        default:
            assert(false && "Cannot dump this kind of node yet!");
            break;

        case Ast::AST_ArrayRangeAttrib:
        case Ast::AST_ScalarRangeAttrib:
            dumpRangeAttrib(cast<RangeAttrib>(node));
            break;
        };
    }
    indentLevel = savedLevel;
    return S;
}

