//===-- ast/Type.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Type.h"
#include "comma/ast/Decl.h"
#include <algorithm>

using namespace comma;

//===----------------------------------------------------------------------===//
// SignatureType

SignatureType::SignatureType(SignatureDecl *decl)
    : ModelType(AST_SignatureType, decl->getIdInfo()), sigoid(decl)
{
    deletable = false;
}

SignatureType::SignatureType(VarietyDecl *decl,
                             DomainType **args, unsigned numArgs)
    : ModelType(AST_SignatureType, decl->getIdInfo()), sigoid(decl)
{
    deletable = false;
    arguments = new DomainType*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

Sigoid *SignatureType::getDeclaration() const
{
    return sigoid;
}

SignatureDecl *SignatureType::getSignature() const
{
    return llvm::dyn_cast<SignatureDecl>(sigoid);
}

VarietyDecl *SignatureType::getVariety() const
{
    return llvm::dyn_cast<VarietyDecl>(sigoid);
}

unsigned SignatureType::getArity() const
{
    VarietyDecl *variety = getVariety();
    if (variety)
        return variety->getArity();
    return 0;
}

DomainType *SignatureType::getActualParameter(unsigned n) const
{
    assert(isParameterized() &&
           "Cannot fetch parameter from non-parameterized type!");
    assert(n < getArity() && "Parameter index out of range!");
    return arguments[n];
}

void SignatureType::Profile(llvm::FoldingSetNodeID &id,
                            DomainType **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// ParameterizedType

ParameterizedType::ParameterizedType(AstKind              kind,
                                     IdentifierInfo      *idInfo,
                                     AbstractDomainType **formalArgs,
                                     unsigned             arity)
    : ModelType(kind, idInfo),
      numFormals(arity)
{
    assert(kind == AST_VarietyType || kind == AST_FunctorType);
    formals = new AbstractDomainType*[arity];
    std::copy(formalArgs, formalArgs + arity, formals);
}

AbstractDomainType *ParameterizedType::getFormalDomain(unsigned i) const
{
    assert(i < getArity() && "Formal domain index out of bounds!");
    return formals[i];
}

SignatureType *ParameterizedType::getFormalType(unsigned i) const
{
    return getFormalDomain(i)->getSignature();
}

IdentifierInfo *ParameterizedType::getFormalIdInfo(unsigned i) const
{
    return getFormalDomain(i)->getIdInfo();
}

int ParameterizedType::getSelectorIndex(IdentifierInfo *selector) const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        if (getFormalIdInfo(i) == selector)
            return i;
    }
    return -1;
}

//===----------------------------------------------------------------------===//
// VarietyType

VarietyType::VarietyType(AbstractDomainType **formalArguments,
                         VarietyDecl *variety, unsigned arity)
    : ParameterizedType(AST_VarietyType,
                        variety->getIdInfo(), formalArguments, arity),
      variety(variety)
{
    // We are owned by the corresponding variety and so we cannot be deleted
    // independently.
    deletable = false;
}

VarietyType::~VarietyType()
{
    delete[] formals;
}

//===----------------------------------------------------------------------===//
// FunctorType

FunctorType::FunctorType(AbstractDomainType **formalArguments,
                         FunctorDecl *functor, unsigned arity)
    : ParameterizedType(AST_FunctorType,
                        functor->getIdInfo(), formalArguments, arity),
      functor(functor)
{
    // We are owned by the corresponding functor and so we cannot be deleted
    // independently.
    deletable = false;
}

FunctorType::~FunctorType()
{
    delete[] formals;
}

//===----------------------------------------------------------------------===//
// DomainType

DomainType::DomainType(AstKind kind, IdentifierInfo *idInfo, ModelDecl *decl)
    : ModelType(kind, idInfo), modelDecl(decl)
{
    assert(this->denotesDomainType());
}

DomainType::DomainType(AstKind kind, IdentifierInfo *idInfo, SignatureType *sig)
    : ModelType(kind, idInfo), signatureType(sig)
{
    assert(this->denotesDomainType());
}

bool DomainType::isAbstract() const
{
    return astNode->getKind() == AST_SignatureType;
}

ModelDecl *DomainType::getDeclaration() const
{
    if (isAbstract() && getKind() != AST_PercentType)
        return signatureType->getDeclaration();
    else
        return modelDecl;
}

Domoid *DomainType::getDomoid() const
{
    return llvm::dyn_cast<Domoid>(modelDecl);
}

DomainDecl *DomainType::getDomain() const
{
    return llvm::dyn_cast<DomainDecl>(modelDecl);
}

FunctorDecl *DomainType::getFunctor() const
{
    return llvm::dyn_cast<FunctorDecl>(modelDecl);
}

//===----------------------------------------------------------------------===//
// ConcreteDomainType.

ConcreteDomainType::ConcreteDomainType(DomainDecl *decl)
    : DomainType(AST_ConcreteDomainType, decl->getIdInfo(), decl),
      arguments(0)
{
    deletable = false;
}

ConcreteDomainType::ConcreteDomainType(FunctorDecl *decl,
                                       DomainType **args,
                                       unsigned     numArgs)
    : DomainType(AST_ConcreteDomainType, decl->getIdInfo(), decl)
{
    deletable = false;
    arguments = new DomainType*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

unsigned ConcreteDomainType::getArity() const
{
    FunctorDecl *functor = getFunctor();
    if (functor) return functor->getArity();
    return 0;
}

DomainType *ConcreteDomainType::getActualParameter(unsigned i) const
{
    assert(i < getArity() && "Index out of range!");
    return arguments[i];
}

void ConcreteDomainType::Profile(llvm::FoldingSetNodeID &id,
                                 DomainType **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// FunctionType.

FunctionType::FunctionType(IdentifierInfo **formals,
                           DomainType     **argTypes,
                           unsigned         numArgs,
                           DomainType      *returnType)
    : Type(AST_FunctionType),
      returnType(returnType),
      numArgs(numArgs)
{
    selectors = new IdentifierInfo*[numArgs];
    argumentTypes = new DomainType*[numArgs];
    std::copy(formals, formals + numArgs, selectors);
    std::copy(argTypes, argTypes + numArgs, argumentTypes);
}

bool FunctionType::selectorsMatch(const FunctionType *ftype) const
{
    unsigned arity = getArity();
    if (ftype->getArity() == arity) {
        for (unsigned i = 0; i < arity; ++i)
            if (getSelector(i) != ftype->getSelector(i))
                return false;
        return true;
    }
    return false;
}

bool FunctionType::equals(const FunctionType *ftype) const
{
    unsigned arity = getArity();

    if (arity != ftype->getArity())
        return false;

    if (getReturnType() != ftype->getReturnType())
        return false;

    for (unsigned i = 0; i < arity; ++i)
        if (getArgType(i) != ftype->getArgType(i))
            return false;

    return true;
}
