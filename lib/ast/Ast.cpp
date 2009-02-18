//===-- ast/Ast.cpp ------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Ast.h"
#include "llvm/Support/Casting.h"
#include <cstring>
#include <cassert>
#include <iostream>

using namespace comma;
using llvm::dyn_cast;

//===----------------------------------------------------------------------===//
// DeclarativeRegion

void DeclarativeRegion::addDecl(Decl *decl) {
    IdentifierInfo *name = decl->getIdInfo();
    declarations.insert(DeclarationTable::value_type(name, decl));
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
            declarations.erase(iter);
            return true;
        }
    return false;
}

const Decl *DeclarativeRegion::asDecl() const
{
    switch (declKind) {
    default:
        assert(false && "Unknown declaration type!");
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
    case Ast::AST_AbstractDomainDecl:
        return static_cast<const AbstractDomainDecl*>(this);
    }
}

Decl *DeclarativeRegion::asDecl()
{
    return const_cast<Decl*>(
        const_cast<const DeclarativeRegion *>(this)->asDecl());
}

