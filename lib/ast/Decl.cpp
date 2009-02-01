//===-- ast/Decl.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/AstRewriter.h"
#include <algorithm>

using namespace comma;
using llvm::dyn_cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Decl

DeclarativeRegion *Decl::asDeclarativeRegion()
{
    switch (getKind()) {
    default:
        return 0;
    case AST_SignatureDecl:
        return static_cast<SignatureDecl*>(this);
    case AST_VarietyDecl:
        return static_cast<VarietyDecl*>(this);
    case AST_DomainDecl:
        return static_cast<DomainDecl*>(this);
    case AST_FunctorDecl:
        return static_cast<FunctorDecl*>(this);
    case AST_AddDecl:
        return static_cast<AddDecl*>(this);
    case AST_FunctionDecl:
        return static_cast<FunctionDecl*>(this);
    }
}

//===----------------------------------------------------------------------===//
// ModelDecl
ModelDecl::ModelDecl(AstKind kind, IdentifierInfo *percentId)
    : Decl(kind),
      location()
{
    assert(std::strcmp(percentId->getString(), "%") == 0 &&
           "Percent IdInfo not == \"%\"!");
    percent = DomainType::getPercent(percentId, this);
}

ModelDecl::ModelDecl(AstKind         kind,
                     IdentifierInfo *percentId,
                     IdentifierInfo *info,
                     const Location &loc)
    : Decl(kind, info),
      location(loc)
{
    assert(std::strcmp(percentId->getString(), "%") == 0 &&
           "Percent IdInfo not == \"%\"!");
    percent = DomainType::getPercent(percentId, this);
}

//===----------------------------------------------------------------------===//
// Sigoid

Sigoid::Sigoid(AstKind kind, DomainType *percent)
    : ModelDecl(kind, percent->getIdInfo()),
      DeclarativeRegion(kind)
{
    assert(percent->denotesPercent());
}

void Sigoid::addSupersignature(SignatureType *supersignature)
{
    if (directSupers.insert(supersignature)) {
        Sigoid *superDecl = supersignature->getDeclaration();
        AstRewriter rewriter;

        // Rewrite the percent node of the super signature to that of this
        // signature.
        rewriter.addRewrite(superDecl->getPercent(), getPercent());

        // If the supplied super signature is parameterized, install rewrites
        // mapping the formal parameters of the signature to the actual
        // parameters of the type.
        if (VarietyDecl *variety = superDecl->getVariety()) {
            unsigned arity = variety->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                DomainType *formal = variety->getFormalDomain(i);
                DomainType *actual = supersignature->getActualParameter(i);
                rewriter.addRewrite(formal, actual);
            }
        }

        sig_iterator iter = superDecl->beginSupers();
        sig_iterator endIter = superDecl->endSupers();
        for ( ; iter != endIter; ++iter) {
            SignatureType *rewrite = rewriter.rewrite(*iter);
            supersignatures.insert(rewrite);
        }
        supersignatures.insert(supersignature);
    }
}

//===----------------------------------------------------------------------===//
// SignatureDecl
SignatureDecl::SignatureDecl(IdentifierInfo *percentId)
    : Sigoid(AST_SignatureDecl, percentId)
{
    canonicalType = new SignatureType(this);
}

SignatureDecl::SignatureDecl(IdentifierInfo *percentId,
                             IdentifierInfo *info,
                             const Location &loc)
    : Sigoid(AST_SignatureDecl, percentId, info, loc)
{
    canonicalType = new SignatureType(this);
}

SignatureDecl::SignatureDecl(DomainDecl *domain)
    : Sigoid(AST_SignatureDecl, domain->getPercent())
{
    canonicalType = new SignatureType(this);
    // Make the given domain this signatures declarative parent.
    setParent(domain);
}

SignatureDecl::SignatureDecl(FunctorDecl *functor)
    : Sigoid(AST_SignatureDecl, functor->getPercent())
{
    canonicalType = new SignatureType(this);
    // Make the given functor this signatures declarative parent.
    setParent(functor);
}

bool SignatureDecl::isPrincipleSignature() const
{
    const DeclarativeRegion *region = getParent();
    // FIXME:  Once signatures are always declared in their proper context
    // we should always have a parent region.
    if (region) {
        const Decl *decl = region->asDecl();
        return isa<DomainDecl>(decl) || isa<FunctorDecl>(decl);
    }
    return false;
}

//===----------------------------------------------------------------------===//
// VarietyDecl

VarietyDecl::VarietyDecl(IdentifierInfo  *percentId,
                         DomainType     **formals,
                         unsigned         arity)
    : Sigoid(AST_VarietyDecl, percentId)
{
    varietyType = new VarietyType(formals, this, arity);
}

VarietyDecl::VarietyDecl(IdentifierInfo *percentId,
                         IdentifierInfo *name,
                         Location        loc,
                         DomainType    **formals,
                         unsigned        arity)
    : Sigoid(AST_VarietyDecl, percentId, name, loc)
{
    varietyType = new VarietyType(formals, this, arity);
}

SignatureType *
VarietyDecl::getCorrespondingType(DomainType **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    SignatureType *type;

    SignatureType::Profile(id, args, numArgs);
    type = types.FindNodeOrInsertPos(id, insertPos);
    if (type) return type;

    type = new SignatureType(this, args, numArgs);
    types.InsertNode(type, insertPos);
    return type;
}

SignatureType *VarietyDecl::getCorrespondingType()
{
    VarietyType *thisType = getType();
    DomainType **formals = reinterpret_cast<DomainType**>(thisType->formals);
    return getCorrespondingType(formals, getArity());
}

//===----------------------------------------------------------------------===//
// Domoid

Domoid::Domoid(AstKind         kind,
               IdentifierInfo *percentId,
               IdentifierInfo *idInfo,
               Location        loc)
    : ModelDecl(kind, percentId, idInfo, loc) { }

//===----------------------------------------------------------------------===//
// AddDecl

// An AddDecl's declarative region is a sub-region of its parent domain decl.
AddDecl::AddDecl(DomainDecl *domain)
    : Decl(AST_AddDecl),
      DeclarativeRegion(AST_AddDecl, domain) { }

AddDecl::AddDecl(FunctorDecl *functor)
    : Decl(AST_AddDecl),
      DeclarativeRegion(AST_AddDecl, functor) { }

bool AddDecl::implementsDomain() const
{
    return isa<DomainDecl>(getParent()->asDecl());
}

bool AddDecl::implementsFunctor() const
{
    return isa<FunctorDecl>(getParent()->asDecl());
}

DomainDecl *AddDecl::getImplementedDomain()
{
    return dyn_cast<DomainDecl>(getParent()->asDecl());
}

FunctorDecl *AddDecl::getImplementedFunctor()
{
    return dyn_cast<FunctorDecl>(getParent()->asDecl());
}

//===----------------------------------------------------------------------===//
// DomainDecl
DomainDecl::DomainDecl(IdentifierInfo *percentId,
                       IdentifierInfo *name,
                       const Location &loc)
    : Domoid(AST_DomainDecl, percentId, name, loc),
      DeclarativeRegion(AST_DomainDecl)
{
    canonicalType      = new DomainType(this);
    principleSignature = new SignatureDecl(this);
    implementation     = new AddDecl(this);
}

//===----------------------------------------------------------------------===//
// FunctorDecl

FunctorDecl::FunctorDecl(IdentifierInfo *percentId,
                         IdentifierInfo *name,
                         Location        loc,
                         DomainType    **formals,
                         unsigned        arity)
    : Domoid(AST_FunctorDecl, percentId, name, loc),
      DeclarativeRegion(AST_FunctorDecl)
{
    functor            = new FunctorType(formals, this, arity);
    principleSignature = new SignatureDecl(this);
    implementation     = new AddDecl(this);
}

DomainType *
FunctorDecl::getCorrespondingType(DomainType **args, unsigned numArgs)
{
    llvm::FoldingSetNodeID id;
    void *insertPos = 0;
    DomainType *type;

    DomainType::Profile(id, args, numArgs);
    type = types.FindNodeOrInsertPos(id, insertPos);
    if (type) return type;

    type = new DomainType(this, args, numArgs);
    types.InsertNode(type, insertPos);
    return type;
}

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
AbstractDomainDecl::AbstractDomainDecl(IdentifierInfo *name,
                                       SignatureType  *type,
                                       Location        loc)
    : Domoid(AST_AbstractDomainDecl,
             type->getDeclaration()->getPercent()->getIdInfo(), name, loc),
      signature(type)
{
    abstractType = new DomainType(this);
}

//===----------------------------------------------------------------------===//
// SubroutineDecl

SubroutineDecl::SubroutineDecl(AstKind            kind,
                               IdentifierInfo    *name,
                               SubroutineType    *type,
                               Location           loc,
                               DeclarativeRegion *parent)
    : Decl(kind, name),
      DeclarativeRegion(kind, parent),
      routineType(type),
      location(loc)
{
    assert(this->denotesSubroutineDecl());

    // Create declarations for this subroutines formal parameters and retain
    // them in the declarative region.
    unsigned arity = routineType->getArity();
    paramDecls = new ValueDecl*[arity];
    for (unsigned i = 0; i < arity; ++i) {
        IdentifierInfo *name = routineType->getSelector(i);
        DomainType     *type = routineType->getArgType(i);
        ValueDecl    *formal = new ValueDecl(name, type);
        addDecl(formal);
        paramDecls[i] = formal;
    }
}
