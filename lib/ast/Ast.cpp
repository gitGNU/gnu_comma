//===-- ast/Ast.cpp ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "AstDumper.h"
#include "comma/ast/Ast.h"
#include "comma/ast/Qualifier.h"

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
    "PercentDecl",

    "SigInstanceDecl",

    "ParamValueDecl",
    "ObjectDecl",

    "FunctionDecl",
    "ProcedureDecl",
    "EnumLiteral",
    "ImportDecl",

    "FunctionType",
    "IntegerType",
    "ArrayType",
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

//===----------------------------------------------------------------------===//
// Nodes which do not belong to any of the major branches in the AST hierarchy
// (Type, Decl, Expr) have their out or line members define below.

//===----------------------------------------------------------------------===//
// Qualifier
DeclRegion *Qualifier::resolveRegion() {
    DeclRegion *region;

    if (DomainTypeDecl *dom = resolve<DomainTypeDecl>())
        region = dom;
    else if (EnumerationDecl *enumDecl = resolve<EnumerationDecl>())
        region = enumDecl;
    else {
        SigInstanceDecl *sig = resolve<SigInstanceDecl>();
        assert(sig && "Unexpected component in qualifier!");
        region = sig->getSigoid()->getPercent();
    }

    return region;
}

