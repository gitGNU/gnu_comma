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
    "VarietyDecl",
    "FunctorDecl",
    "AddDecl",

    "CarrierDecl",
    "DomainValueDecl",
    "AbstractDomainDecl",
    "DomainInstanceDecl",
    "ParamValueDecl",
    "ObjectDecl",
    "EnumerationDecl",
    "IntegerDecl",

    "FunctionDecl",
    "ProcedureDecl",
    "EnumLiteral",
    "ImportDecl",

    "FunctionType",
    "IntegerType",
    "ProcedureType",

    "SignatureType",
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
