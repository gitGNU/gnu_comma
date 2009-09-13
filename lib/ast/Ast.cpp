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
    "IntegerDecl",
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
    "IntegerType",
    "ArrayType",
    "ProcedureType",

    "DomainType",
    "CarrierType",
    "TypedefType",
    "EnumerationType",

    "DeclRefExpr",
    "FunctionCallExpr",
    "IndexedArrayExpr",
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
    IdentifierInfo *idInfo = decls[0]->getIdInfo();
    bool isaFunction = isa<FunctionDecl>(decls[0]);
    verify(idInfo, isaFunction);
}

void SubroutineRef::verify(IdentifierInfo *idInfo, bool isaFunction)
{
    unsigned numDecls = decls.size();
    for (unsigned i = 1; i < numDecls; ++i) {
        SubroutineDecl *srDecl = decls[i];

        // All decls must have the same name.
        assert(srDecl->getIdInfo() == idInfo &&
               "All declarations must have the same identifier!");

        // All decls must either procedures or functions, but not both.
        if (isaFunction)
            assert(isa<FunctionDecl>(srDecl) && "Declaration type mismatch!");
        else
            assert(isa<ProcedureDecl>(srDecl) && "Declaration type mismatch!");
    }
}

void SubroutineRef::addDeclaration(SubroutineDecl *srDecl)
{
    IdentifierInfo *idInfo = srDecl->getIdInfo();
    bool isaFunction = isa<FunctionDecl>(srDecl);
    verify(idInfo, isaFunction);
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
