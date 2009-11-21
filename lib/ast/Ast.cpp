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
#include "comma/ast/SubroutineRef.h"

#include <algorithm>

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
    "EnumSubtypeDecl",
    "IntegerDecl",
    "IntegerSubtypeDecl",
    "ArrayDecl",
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
    "ProcedureType",
    "DomainType",
    "IntegerType",
    "ArrayType",
    "EnumerationType",
    "CarrierType",

    "AggregateExpr",
    "ConversionExpr",
    "DeclRefExpr",
    "FunctionCallExpr",
    "IndexedArrayExpr",
    "InjExpr",
    "IntegerLiteral",
    "PrjExpr",
    "StringLiteral",

    "FirstAE",
    "FirstArrayAE",
    "LastArrayAE",
    "LastAE",

    "AssignmentStmt",
    "BlockStmt",
    "IfStmt",
    "ProcedureCallStmt",
    "ReturnStmt",
    "StmtSequence",
    "WhileStmt",
    "PragmaStmt",

    "KeywordSelector",
    "Qualifier",
    "Range",
    "ArrayRangeAttrib",
    "ScalarRangeAttrib",
    "SubroutineRef",
    "TypeRef"
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

//===----------------------------------------------------------------------===//
// SubroutineRef

void SubroutineRef::verify()
{
    assert(!decls.empty() && "Empty SubroutineRef!");
    SubroutineDecl *elem = decls[0];
    IdentifierInfo *name = elem->getIdInfo();
    bool isaFunction = isa<FunctionDecl>(elem);
    unsigned numDecls = numDeclarations();

    for (unsigned i = 1; i < numDecls; ++i) {
        SubroutineDecl *cursor = decls[i];
        verify(cursor, name, isaFunction);
    }
}

void SubroutineRef::verify(SubroutineDecl *decl,
                           IdentifierInfo *name, bool isaFunction)
{
    assert(decl->getIdInfo() == name &&
           "All declarations must have the same identifier!");
    if (isaFunction)
        assert(isa<FunctionDecl>(decl) && "Declaration type mismatch!");
    else
        assert(isa<ProcedureDecl>(decl) && "Declaration type mismatch!");
}

void SubroutineRef::addDeclaration(SubroutineDecl *srDecl)
{
    if (decls.empty())
        decls.push_back(srDecl);
    else {
        IdentifierInfo *name = getIdInfo();
        bool isaFunction = referencesFunctions();
        verify(srDecl, name, isaFunction);
        decls.push_back(srDecl);
    }
}

bool SubroutineRef::contains(const SubroutineDecl *srDecl) const
{
    const_iterator I = std::find(begin(), end(), srDecl);
    if (I != end())
        return true;
    return false;
}

bool SubroutineRef::contains(const SubroutineType *srType) const
{
    for (const_iterator I = begin(); I != end(); ++I) {
        SubroutineDecl *target = *I;
        if (target->getType() == srType)
            return true;
    }
    return false;
}

bool SubroutineRef::keepSubroutinesWithArity(unsigned arity)
{
    DeclVector::iterator I = decls.begin();
    while (I != decls.end()) {
        SubroutineDecl *decl = *I;
        if (decl->getArity() != arity)
            I = decls.erase(I);
        else
            ++I;
    }
    return !decls.empty();
}
