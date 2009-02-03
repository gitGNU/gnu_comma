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
using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// SignatureType

SignatureType::SignatureType(SignatureDecl *decl)
    : ModelType(AST_SignatureType, decl->getIdInfo(), decl)
{
    deletable = false;
}

SignatureType::SignatureType(VarietyDecl *decl,
                             DomainType **args, unsigned numArgs)
    : ModelType(AST_SignatureType, decl->getIdInfo(), decl)
{
    deletable = false;
    arguments = new DomainType*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

Sigoid *SignatureType::getDeclaration() const
{
    return dyn_cast<Sigoid>(declaration);
}

SignatureDecl *SignatureType::getSignature() const
{
    return dyn_cast<SignatureDecl>(declaration);
}

VarietyDecl *SignatureType::getVariety() const
{
    return dyn_cast<VarietyDecl>(declaration);
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

ParameterizedType::ParameterizedType(AstKind         kind,
                                     IdentifierInfo *idInfo,
                                     ModelDecl      *decl,
                                     DomainType    **formalArgs,
                                     unsigned        arity)
    : ModelType(kind, idInfo, decl),
      numFormals(arity)
{
    assert(kind == AST_VarietyType || kind == AST_FunctorType);
    formals = new DomainType*[arity];
    std::copy(formalArgs, formalArgs + arity, formals);
}

DomainType *ParameterizedType::getFormalDomain(unsigned i) const
{
    assert(i < getArity() && "Formal domain index out of bounds!");
    return formals[i];
}

SignatureType *ParameterizedType::getFormalType(unsigned i) const
{
    AbstractDomainDecl *decl = getFormalDomain(i)->getAbstractDecl();
    return decl->getSignatureType();
}

IdentifierInfo *ParameterizedType::getFormalIdInfo(unsigned i) const
{
    return getFormalDomain(i)->getIdInfo();
}

int ParameterizedType::getKeywordIndex(IdentifierInfo *keyword) const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        if (getFormalIdInfo(i) == keyword)
            return i;
    }
    return -1;
}

//===----------------------------------------------------------------------===//
// VarietyType

VarietyType::VarietyType(DomainType **formalArguments,
                         VarietyDecl *variety,
                         unsigned     arity)
    : ParameterizedType(AST_VarietyType,
                        variety->getIdInfo(),
                        variety,
                        formalArguments, arity)
{
    // We are owned by the corresponding variety and so we cannot be deleted
    // independently.
    deletable = false;
}

VarietyType::~VarietyType()
{
    delete[] formals;
}

VarietyDecl *VarietyType::getDeclaration() const
{
    return dyn_cast<VarietyDecl>(declaration);
}

//===----------------------------------------------------------------------===//
// FunctorType

FunctorType::FunctorType(DomainType **formalArguments,
                         FunctorDecl *functor,
                         unsigned     arity)
    : ParameterizedType(AST_FunctorType,
                        functor->getIdInfo(),
                        functor,
                        formalArguments, arity)
{
    // We are owned by the corresponding functor and so we cannot be deleted
    // independently.
    deletable = false;
}

FunctorType::~FunctorType()
{
    delete[] formals;
}

FunctorDecl *FunctorType::getDeclaration() const
{
    return dyn_cast<FunctorDecl>(declaration);
}

//===----------------------------------------------------------------------===//
// DomainType

DomainType::DomainType(DomainDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl),
      arguments(0)
{
    deletable = false;
}

DomainType::DomainType(FunctorDecl *decl,
                       DomainType **args,
                       unsigned     numArgs)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{
    deletable = false;
    arguments = new DomainType*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

DomainType::DomainType(AbstractDomainDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl),
      arguments(0)
{
    deletable = false;
}

DomainType::DomainType(IdentifierInfo *percentId, ModelDecl *model)
    : ModelType(AST_DomainType, percentId, model),
      arguments(0)
{
    deletable = false;
}

DomainType *DomainType::getPercent(IdentifierInfo *percentId, ModelDecl *decl)
{
    return new DomainType(percentId, decl);
}

bool DomainType::denotesPercent() const
{
    return this == declaration->getPercent();
}

Domoid *DomainType::getDomoidDecl() const
{
    return dyn_cast<Domoid>(declaration);
}

DomainDecl *DomainType::getDomainDecl() const
{
    return dyn_cast<DomainDecl>(declaration);
}

FunctorDecl *DomainType::getFunctorDecl() const
{
    return dyn_cast<FunctorDecl>(declaration);
}

AbstractDomainDecl *DomainType::getAbstractDecl() const
{
    return dyn_cast<AbstractDomainDecl>(declaration);
}

unsigned DomainType::getArity() const
{
    FunctorDecl *functor = getFunctorDecl();
    if (functor) return functor->getArity();
    return 0;
}

DomainType *DomainType::getActualParameter(unsigned i) const
{
    assert(i < getArity() && "Index out of range!");
    return arguments[i];
}

void DomainType::Profile(llvm::FoldingSetNodeID &id,
                         DomainType **args, unsigned numArgs)
{
    for (unsigned i = 0; i < numArgs; ++i)
        id.AddPointer(args[i]);
}

//===----------------------------------------------------------------------===//
// SubroutineType.

SubroutineType::SubroutineType(AstKind          kind,
                               IdentifierInfo **formals,
                               DomainType     **argTypes,
                               unsigned         numArgs)
    : Type(kind),
      numArgs(numArgs)
{
    assert(this->denotesSubroutineType());
    keywords = new IdentifierInfo*[numArgs];
    argumentTypes = new DomainType*[numArgs];
    std::copy(formals, formals + numArgs, keywords);
    std::copy(argTypes, argTypes + numArgs, argumentTypes);
}

bool SubroutineType::keywordsMatch(const SubroutineType *routineType) const
{
    unsigned arity = getArity();
    if (routineType->getArity() == arity) {
        for (unsigned i = 0; i < arity; ++i)
            if (getKeyword(i) != routineType->getKeyword(i))
                return false;
        return true;
    }
    return false;
}

bool SubroutineType::equals(const SubroutineType *routineType) const
{
    unsigned arity = getArity();

    if (arity != routineType->getArity())
        return false;

    for (unsigned i = 0; i < arity; ++i)
        if (getArgType(i) != routineType->getArgType(i))
            return false;

    if (const FunctionType *thisType = dyn_cast<FunctionType>(this)) {
        const FunctionType *otherType = dyn_cast<FunctionType>(routineType);

        if (!otherType)
            return false;

        if (thisType->getReturnType() != otherType->getReturnType())
            return false;
    }

    // This must be a function type.  The types are therefore equal if the
    // target is also a procedure.
    return isa<ProcedureType>(routineType);
}
