//===-- ast/Ast.cpp ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Ast.h"
#include <iostream>

using namespace comma;

const char *Ast::kindStrings[LAST_AstKind] = {
    "SignatureDecl",
    "DomainDecl",
    "AbstractDomainDecl",
    "DomainInstanceDecl",
    "VarietyDecl",
    "FunctorDecl",
    "CarrierDecl",
    "EnumerationDecl",
    "AddDecl",
    "FunctionDecl",
    "ProcedureDecl",
    "ParamValueDecl",
    "EnumerationLiteral",
    "ObjectDecl",
    "ImportDecl",

    "SignatureType",
    "VarietyType",
    "FunctorType",
    "DomainType",
    "CarrierType",
    "FunctionType",
    "ProcedureType",
    "EnumerationType",

    "DeclRefExpr",
    "FunctionCallExpr",
    "InjExpr",
    "KeywordSelector",
    "PrjExpr",

    "AssignmentStmt",
    "BlockStmt",
    "IfStmt",
    "ProcedureCallStmt",
    "ReturnStmt",
    "StmtSequence",

    "Qualifier"
};

void Ast::dump()
{
    std::cerr << '<'
              << getKindString()
              << ' ' << std::hex << uintptr_t(this)
              << '>';
}
