//===-- ast/Type.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

// FIXME:  Provide access to the global IdentifierPool.  Remove once this
// resource has a better interface.
#include "comma/basic/IdentifierPool.h"

#include "comma/ast/Type.h"
#include "comma/ast/Decl.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include <cstring>

using namespace comma;

//===----------------------------------------------------------------------===//
// ModelType methods.

ModelType::ModelType(AstKind kind, ModelDecl *decl,
                     ModelType **args, unsigned numArgs)
    : Type(kind), declaration(decl), arity(numArgs)
{
    assert(this->denotesModelType());
    arguments = new ModelType*[numArgs];
    memcpy(arguments, args, sizeof(DomainType*) * numArgs);
}

IdentifierInfo *ModelType::getIdInfo() const
{
    return getDeclaration()->getIdInfo();
}

//===----------------------------------------------------------------------===//
// SignatureType methods.

SignatureType::SignatureType(SignatureDecl *decl)
    : ModelType(AST_SignatureType, decl)
{
    populateSignatureTable();
}

SignatureType::SignatureType(VarietyDecl *decl,
                             ModelType **args, unsigned numArgs)
  : ModelType(AST_SignatureType, decl, args, numArgs)
{
    populateSignatureTable();
}

// Inserts a signature into the signature table.  Checks that the provided type
// does not already exist in the table.
void SignatureType::insertSupersignature(SignatureType *sig)
{
    sig_iterator begin = beginDirectSupers();
    sig_iterator end = endDirectSupers();
    if (std::find(begin, end, sig) == end)
        directSupers.push_back(sig);
}

void SignatureType::addSupersignature(SignatureType *sig)
{
    AstRewriter rewriter;

    if (VarietyDecl *sig = llvm::dyn_cast<VarietyDecl>(getDeclaration())) {
        // We are a parametrized type.  Install rewrite rules which map formal
        // parameters to our actual arguments.
        for (unsigned i = 0; i < sig->getArity(); ++i) {
            DomainType *formal = sig->getFormalDomain(i);
            ModelType *actual = this->getArgument(i);
            rewriter.addRewrite(formal, actual);
        }
    }

    // FIXME: If the rewritten node is a duplicate the pointer is leaked.
    SignatureType *super = rewriter.rewrite(sig);
    insertSupersignature(super);
}

void SignatureType::populateSignatureTable()
{
    Sigoid *decl = getDeclaration();
    SignatureDecl::sig_iterator iter = decl->beginDirectSupers();
    SignatureDecl::sig_iterator endIter = decl->endDirectSupers();
    for ( ; iter != endIter; ++iter)
        addSupersignature(*iter);
}

Sigoid *SignatureType::getDeclaration() const
{
    return static_cast<Sigoid*>(declaration);
}

SignatureDecl *SignatureType::getSignature() const
{
    return llvm::dyn_cast<SignatureDecl>(declaration);
}

VarietyDecl *SignatureType::getVariety() const
{
    return llvm::dyn_cast<VarietyDecl>(declaration);
}

bool SignatureType::has(SignatureType *sig)
{
    if (sig == this) return true;

    llvm::df_iterator<SignatureType*> iter = llvm::df_begin(this);
    for ( ; iter != llvm::df_end(this); ++iter)
        if (*iter == sig) return true;

    return false;
}

void SignatureType::Profile(llvm::FoldingSetNodeID &id,
                            ModelType **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// DomainType methods.

DomainType::DomainType(DomainDecl *decl)
    : ModelType(AST_DomainType, decl) { }

DomainType::DomainType(FunctorDecl *decl,
                       ModelType **args, unsigned numArgs)
    : ModelType(AST_DomainType, decl, args, numArgs) { }

Domoid *DomainType::getDeclaration() const
{
    return static_cast<Domoid*>(declaration);
}

DomainDecl *DomainType::getDomain() const
{
    return llvm::dyn_cast<DomainDecl>(declaration);
}

FunctorDecl *DomainType::getFunctor() const
{
    return llvm::dyn_cast<FunctorDecl>(declaration);
}

SignatureType *DomainType::getSignature() const
{
    return getDeclaration()->getPrincipleSignature();
}

bool DomainType::has(SignatureType *type)
{
    return getSignature()->has(type);
}

void DomainType::Profile(llvm::FoldingSetNodeID &id,
                         ModelType **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// DomainType.

IdentifierInfo *PercentType::getIdInfo() const
{
    return &IdentifierPool::getIdInfo("%");
}

bool PercentType::has(SignatureType *type)
{
     ModelDecl *decl = getDeclaration();

    if (SignatureDecl *sig = llvm::dyn_cast<SignatureDecl>(decl))
        return sig->getCorrespondingType()->has(type);
    else if (Domoid *dom = llvm::dyn_cast<Domoid>(decl))
        return dom->getPrincipleSignature()->has(type);
    else if (VarietyDecl *variety = llvm::dyn_cast<VarietyDecl>(decl)) {
        // For a variety V(X0 : T0, ..., Xn : Tn), check against the signature
        // V(X0, ..., Xn).
        SignatureType *sig = variety->getCorrespondingType(arguments, arity);
        return sig->has(type);
    }
    else {
        assert(false && "Unknown type declaration!");
        return false;
    }
}
