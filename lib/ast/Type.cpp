//===-- ast/Type.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
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
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{
    deletable = false;
}

DomainType::DomainType(DomainInstanceDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{
    deletable = false;
}

DomainType::DomainType(AbstractDomainDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{
    deletable = false;
}

DomainType::DomainType(IdentifierInfo *percentId, ModelDecl *model)
    : ModelType(AST_DomainType, percentId, model)
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

DomainInstanceDecl *DomainType::getInstanceDecl() const
{
    return dyn_cast<DomainInstanceDecl>(declaration);
}

AbstractDomainDecl *DomainType::getAbstractDecl() const
{
    return dyn_cast<AbstractDomainDecl>(declaration);
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
    parameterInfo = new ParamInfo[numArgs];
    std::copy(formals, formals + numArgs, keywords);

    for (unsigned i = 0; i < numArgs; ++i)
        parameterInfo[i].setPointer(argTypes[i]);
}

SubroutineType::SubroutineType(AstKind          kind,
                               IdentifierInfo **formals,
                               DomainType     **argTypes,
                               ParameterMode   *modes,
                               unsigned         numArgs)
    : Type(kind),
      numArgs(numArgs)
{
    assert(this->denotesSubroutineType());
    keywords = new IdentifierInfo*[numArgs];
    parameterInfo = new ParamInfo[numArgs];
    std::copy(formals, formals + numArgs, keywords);

    for (unsigned i = 0; i < numArgs; ++i) {
        parameterInfo[i].setPointer(argTypes[i]);
        setParameterMode(modes[i], i);
    }
}

DomainType *SubroutineType::getArgType(unsigned i) const
{
    assert(i < getArity() && "Index out of range!");
    return parameterInfo[i].getPointer();
}

int SubroutineType::getKeywordIndex(IdentifierInfo *key) const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        if (getKeyword(i) == key)
            return i;
    }
    return -1;
}

ParameterMode SubroutineType::getParameterMode(unsigned i) const
{
    assert(i < getArity() && "Index out of range!");
    ParameterMode mode = static_cast<ParameterMode>(parameterInfo[i].getInt());
    if (mode == MODE_DEFAULT)
        return MODE_IN;
    else
        return mode;
}

void SubroutineType::setParameterMode(ParameterMode mode, unsigned i)
{
    assert(i < getArity() && "Index out of range!");

    if ((mode == MODE_OUT || mode == MODE_IN_OUT) && isa<FunctionType>(this))
        assert(false && "Only procedures can have `out' parameter modes!");

    parameterInfo[i].setInt(mode);
}

IdentifierInfo **SubroutineType::getKeywordArray() const
{
    if (getArity() == 0) return 0;

    return &keywords[0];
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

bool SubroutineType::equals(const Type *type) const
{
    const SubroutineType *routineType = dyn_cast<SubroutineType>(type);
    unsigned arity = getArity();

    if (!routineType) return false;

    if (arity != routineType->getArity())
        return false;

    for (unsigned i = 0; i < arity; ++i)
        if (getArgType(i) != routineType->getArgType(i))
            return false;

    if (const FunctionType *thisType = dyn_cast<FunctionType>(this)) {
        const FunctionType *otherType = dyn_cast<FunctionType>(routineType);

        if (!otherType)
            return false;

        if (thisType->getReturnType()->equals(otherType->getReturnType()))
            return true;

        return false;
    }

    // This must be a function type.  The types are therefore equal if the
    // target is also a procedure.
    return isa<ProcedureType>(routineType);
}
