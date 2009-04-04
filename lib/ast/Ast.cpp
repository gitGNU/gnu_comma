//===-- ast/Ast.cpp ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Ast.h"
#include "comma/ast/AstRewriter.h"
#include "llvm/Support/Casting.h"
#include <cstring>
#include <cassert>
#include <iostream>

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

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

//===----------------------------------------------------------------------===//
// DeclarativeRegion

void DeclarativeRegion::addDecl(Decl *decl) {
    IdentifierInfo *name = decl->getIdInfo();
    declarations.insert(DeclarationTable::value_type(name, decl));
    notifyObserversOfAddition(decl);
}

void DeclarativeRegion::addDeclarationUsingRewrites(const AstRewriter &rewrites,
                                                    Decl *decl)
{
    Decl *newDecl = 0;

    switch (decl->getKind()) {

    default:
        assert(false && "Bad type of declaration!");
        break;

    case Ast::AST_FunctionDecl: {
        FunctionDecl *fdecl = cast<FunctionDecl>(decl);
        FunctionType *ftype = rewrites.rewrite(fdecl->getType());
        newDecl = new FunctionDecl(decl->getIdInfo(), 0, ftype, this);
        break;
    }

    case Ast::AST_ProcedureDecl: {
        ProcedureDecl *pdecl = cast<ProcedureDecl>(decl);
        ProcedureType *ptype = rewrites.rewrite(pdecl->getType());
        newDecl = new ProcedureDecl(decl->getIdInfo(), 0, ptype, this);
        break;
    }

    case Ast::AST_EnumerationDecl:
        // Nothing to do for an enumeration since there are never free variables
        // in such a type.
        newDecl = decl;
        break;
    }
    if (newDecl)
        this->addDecl(newDecl);
}

void
DeclarativeRegion::addDeclarationsUsingRewrites(const AstRewriter &rewrites,
                                                const DeclarativeRegion *region)
{
    ConstDeclIter iter;
    ConstDeclIter endIter = region->endDecls();

    for (iter = region->beginDecls(); iter != endIter; ++iter)
        addDeclarationUsingRewrites(rewrites, iter->second);
}

Decl *DeclarativeRegion::findDecl(IdentifierInfo *name, Type *type)
{
    DeclRange range = findDecls(name);
    for (DeclIter iter = range.first; iter != range.second; ++iter) {
        if (ModelDecl *model = dyn_cast<ModelDecl>(iter->second)) {
            Type *candidateType = model->getType();
            if (candidateType->equals(type))
                return model;
            continue;
        }
        if (SubroutineDecl *srDecl = dyn_cast<SubroutineDecl>(iter->second)) {
            SubroutineType *candidateType = srDecl->getType();
            if (candidateType->equals(type))
                return srDecl;
            continue;
        }
    }
    return 0;
}

Decl *DeclarativeRegion::findDirectDecl(IdentifierInfo *name, Type *type)
{
    Decl *candidate = findDecl(name, type);
    if (candidate && candidate->isDeclaredIn(this))
        return candidate;
    return 0;
}

bool DeclarativeRegion::removeDecl(Decl *decl)
{
    IdentifierInfo *name = decl->getIdInfo();
    DeclRange      range = findDecls(name);
    for (DeclIter iter = range.first; iter != range.second; ++iter)
        if (iter->second == decl) {
            notifyObserversOfRemoval(decl);
            declarations.erase(iter);
            return true;
        }
    return false;
}

const Ast *DeclarativeRegion::asAst() const
{
    switch (regionKind) {
    default:
        assert(false && "Unknown delcarative region kind!");
        return 0;
    case Ast::AST_SignatureDecl:
        return static_cast<const SignatureDecl*>(this);
    case Ast::AST_VarietyDecl:
        return static_cast<const VarietyDecl*>(this);
    case Ast::AST_DomainDecl:
        return static_cast<const DomainDecl*>(this);
    case Ast::AST_FunctorDecl:
        return static_cast<const FunctorDecl*>(this);
    case Ast::AST_FunctionDecl:
        return static_cast<const FunctionDecl*>(this);
    case Ast::AST_ProcedureDecl:
        return static_cast<const ProcedureDecl*>(this);
    case Ast::AST_AbstractDomainDecl:
        return static_cast<const AbstractDomainDecl*>(this);
    case Ast::AST_DomainInstanceDecl:
        return static_cast<const DomainInstanceDecl*>(this);
    case Ast::AST_AddDecl:
        return static_cast<const AddDecl*>(this);
    case Ast::AST_EnumerationDecl:
        return static_cast<const EnumerationDecl*>(this);
    case Ast::AST_BlockStmt:
        return static_cast<const BlockStmt*>(this);
    }
}

Ast *DeclarativeRegion::asAst()
{
    return const_cast<Ast*>(
        const_cast<const DeclarativeRegion *>(this)->asAst());
}

// Default implementation -- do nothing.
void DeclarativeRegion::notifyAddDecl(Decl *decl) { }

// Default implementation -- do nothing.
void DeclarativeRegion::notifyRemoveDecl(Decl *decl) { }

void DeclarativeRegion::notifyObserversOfAddition(Decl *decl)
{
    for (ObserverList::iterator iter = observers.begin();
         iter != observers.end(); ++iter)
        (*iter)->notifyAddDecl(decl);
}

void DeclarativeRegion::notifyObserversOfRemoval(Decl *decl)
{
    for (ObserverList::iterator iter = observers.begin();
         iter != observers.end(); ++iter)
        (*iter)->notifyRemoveDecl(decl);
}
