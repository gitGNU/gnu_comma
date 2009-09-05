//===-- ast/Ast.cpp ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "AstDumper.h"
#include "comma/ast/Ast.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

const char *Ast::kindStrings[LAST_AstKind] = {
    "SignatureDecl",
    "DomainDecl",
    "VarietyDecl",
    "FunctorDecl",
    "AddDecl",

    "CarrierDecl",
    "EnumerationDecl",
    "IntegerDecl",
    "AbstractDomainDecl",
    "DomainInstanceDecl",

    "SigInstanceDecl",

    "ParamValueDecl",
    "ObjectDecl",

    "FunctionDecl",
    "ProcedureDecl",
    "EnumLiteral",
    "ImportDecl",

    "FunctionType",
    "IntegerType",
    "ProcedureType",

    "DomainType",
    "CarrierType",
    "TypedefType",
    "EnumerationType",

    "DeclRefExpr",
    "FunctionCallExpr",
    "InjExpr",
    "IntegerLiteral",
    "KeywordSelector",
    "PrjExpr",

    "AssignmentStmt",
    "BlockStmt",
    "IfStmt",
    "ProcedureCallStmt",
    "ReturnStmt",
    "StmtSequence",
    "WhileStmt",

    "Qualifier",
    "OverloadedDeclName"
};

void Ast::dump()
{
    AstDumper dumper(llvm::errs());
    dumper.dump(this);
    llvm::errs().flush();
}

