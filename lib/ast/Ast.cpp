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
    "EnumLiteral",
    "ObjectDecl",
    "ImportDecl",

    "SignatureType",
    "VarietyType",
    "FunctorType",
    "DomainType",
    "CarrierType",
    "FunctionType",
    "IntegerType",
    "ProcedureType",
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

    "Qualifier",
    "OverloadedDeclName"
};

void Ast::dump(unsigned depth)
{
    dumpSpaces(depth);
    std::cerr << '<'
              << getKindString()
              << ' ' << std::hex << uintptr_t(this)
              << '>';
}

void Ast::dumpSpaces(unsigned n)
{
    while (n--) std::cerr << "  ";
}
