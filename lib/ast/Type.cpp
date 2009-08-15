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
#include <iostream>

using namespace comma;
using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Type

bool Type::isScalarType() const
{
    if (const CarrierType *carrier = dyn_cast<CarrierType>(this))
        return carrier->getRepresentationType()->isScalarType();
    return isa<EnumerationType>(this);
}

//===----------------------------------------------------------------------===//
// CarrierType

Decl *CarrierType::getDeclaration()
{
    return declaration;
}

Type *CarrierType::getRepresentationType()
{
    return declaration->getRepresentationType();
}

const Type *CarrierType::getRepresentationType() const
{
    return declaration->getRepresentationType();
}

IdentifierInfo *CarrierType::getIdInfo() const
{
    return declaration->getIdInfo();
}

const char *CarrierType::getString() const
{
    return declaration->getString();
}

bool CarrierType::equals(const Type *type) const
{
    if (this == type) return true;
    return getRepresentationType()->equals(type);
}

//===----------------------------------------------------------------------===//
// ModelType

Decl *ModelType::getDeclaration()
{
    return declaration;
}

//===----------------------------------------------------------------------===//
// SignatureType

SignatureType::SignatureType(SignatureDecl *decl)
    : ModelType(AST_SignatureType, decl->getIdInfo(), decl)
{ }

SignatureType::SignatureType(VarietyDecl *decl,
                             Type **args, unsigned numArgs)
    : ModelType(AST_SignatureType, decl->getIdInfo(), decl)
{
    arguments = new Type*[numArgs];
    std::copy(args, args + numArgs, arguments);
}

Sigoid *SignatureType::getSigoid()
{
    return dyn_cast<Sigoid>(declaration);
}

const Sigoid *SignatureType::getSigoid() const
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

Type *SignatureType::getActualParameter(unsigned n) const
{
    assert(isParameterized() &&
           "Cannot fetch parameter from non-parameterized type!");
    assert(n < getArity() && "Parameter index out of range!");
    return arguments[n];
}

void SignatureType::Profile(llvm::FoldingSetNodeID &id,
                            Type **args, unsigned numArgs)
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

DomainType *ParameterizedType::getFormalType(unsigned i) const
{
    assert(i < getArity() && "Formal domain index out of bounds!");
    return formals[i];
}

SignatureType *ParameterizedType::getFormalSignature(unsigned i) const
{
    AbstractDomainDecl *decl = getFormalType(i)->getAbstractDecl();
    return decl->getSignatureType();
}

IdentifierInfo *ParameterizedType::getFormalIdInfo(unsigned i) const
{
    return getFormalType(i)->getIdInfo();
}

int ParameterizedType::getKeywordIndex(IdentifierInfo *keyword) const
{
    for (unsigned i = 0; i < getArity(); ++i) {
        if (getFormalIdInfo(i) == keyword)
            return i;
    }
    return -1;
}

SignatureType *ParameterizedType::resolveFormalSignature(Type   **actuals,
                                                         unsigned numActuals)
{
    AstRewriter rewriter;

    // For each actual argument, establish a map from the formal parameter to
    // the actual.
    for (unsigned i = 0; i < numActuals; ++i) {
        Type *formal = getFormalType(i);
        Type *actual = actuals[i];
        rewriter.addRewrite(formal, actual);
    }

    SignatureType *target = getFormalSignature(numActuals);
    return rewriter.rewrite(target);
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
{ }

VarietyType::~VarietyType()
{
    delete[] formals;
}

VarietyDecl *VarietyType::getVarietyDecl()
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
{ }

FunctorType::~FunctorType()
{
    delete[] formals;
}

FunctorDecl *FunctorType::getFunctorDecl()
{
    return dyn_cast<FunctorDecl>(declaration);
}

//===----------------------------------------------------------------------===//
// DomainType

DomainType::DomainType(DomainDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{ }

DomainType::DomainType(DomainInstanceDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{ }

DomainType::DomainType(AbstractDomainDecl *decl)
    : ModelType(AST_DomainType, decl->getIdInfo(), decl)
{ }

DomainType::DomainType(IdentifierInfo *percentId, ModelDecl *model)
    : ModelType(AST_DomainType, percentId, model)
{ }

DomainType *DomainType::getPercent(IdentifierInfo *percentId, ModelDecl *decl)
{
    return new DomainType(percentId, decl);
}

bool DomainType::denotesPercent() const
{
    return this == declaration->getPercent();
}

Decl *DomainType::getDeclaration()
{
    return declaration;
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

bool DomainType::equals(const Type *type) const
{
    if (this == type) return true;

    // Otherwise, the candidate type must be a carrier with a representation
    // equal to this domain.
    if (const CarrierType *carrier = dyn_cast<CarrierType>(type))
        return this == carrier->getRepresentationType();

    return false;
}

void DomainType::dump(unsigned depth)
{
    dumpSpaces(depth++);
    std::cerr << '<' << getKindString()
              << ' ' << std::hex << uintptr_t(this) << ' ';

    if (denotesPercent())
        std::cerr << "%\n";
    else
        std::cerr << getString() << '\n';

    getDeclaration()->dump(depth);
    std::cerr << '>';
}

//===----------------------------------------------------------------------===//
// SubroutineType.

SubroutineType::SubroutineType(AstKind          kind,
                               IdentifierInfo **formals,
                               Type           **argTypes,
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

SubroutineType::SubroutineType(AstKind kind,
                               IdentifierInfo **formals,
                               Type **argTypes,
                               PM::ParameterMode *modes,
                               unsigned numArgs)
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

Type *SubroutineType::getArgType(unsigned i) const
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

PM::ParameterMode SubroutineType::getParameterMode(unsigned i) const
{
    PM::ParameterMode mode = getExplicitParameterMode(i);
    if (mode == PM::MODE_DEFAULT)
        return PM::MODE_IN;
    else
        return mode;
}

PM::ParameterMode SubroutineType::getExplicitParameterMode(unsigned i) const
{
    return static_cast<PM::ParameterMode>(parameterInfo[i].getInt());
}

void SubroutineType::setParameterMode(PM::ParameterMode mode, unsigned i)
{
    assert(i < getArity() && "Index out of range!");

    if ((mode == PM::MODE_OUT || mode == PM::MODE_IN_OUT) &&
        isa<FunctionType>(this))
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
        if (!getArgType(i)->equals(routineType->getArgType(i)))
            return false;

    if (const FunctionType *thisType = dyn_cast<FunctionType>(this)) {
        const FunctionType *otherType = dyn_cast<FunctionType>(routineType);

        if (!otherType)
            return false;

        if (thisType->getReturnType()->equals(otherType->getReturnType()))
            return true;

        return false;
    }

    // This must be a procedure type.  The types are therefore equal if the
    // target is also a procedure.
    return isa<ProcedureType>(routineType);
}

void SubroutineType::dump(unsigned depth)
{
    dumpSpaces(depth++);
    std::cerr << '<' <<  getKindString() << ' '
              << std::hex << uintptr_t(this) << '\n';

    if (numArgs > 0) {
        for (unsigned i = 0; i < numArgs; ++i) {
            dumpSpaces(depth);
            std::cerr << '(' << getKeyword(i)->getString() << " : ";
            switch (getExplicitParameterMode(i)) {
            case PM::MODE_IN:
                std::cerr << "in";
                break;
            case PM::MODE_IN_OUT:
                std::cerr << "in out";
                break;
            case PM::MODE_OUT:
                std::cerr << "out";
                break;
            case PM::MODE_DEFAULT:
                break;
            }
            std::cerr << '\n';
            getArgType(i)->dump(depth + 1);

            std::cerr << ')';
            if (i != numArgs - 1) std::cerr << '\n';
        }
    }

    if (FunctionType *ftype = dyn_cast<FunctionType>(this)) {
        std::cerr << '\n';
        ftype->getReturnType()->dump(depth);
    }
    std::cerr << '>';
}

//===----------------------------------------------------------------------===//
// EnumerationType

bool EnumerationType::equals(const Type *type) const
{
    if (const CarrierType *carrier = dyn_cast<CarrierType>(type))
        return this == carrier->getRepresentationType();

    return this == type;
}

Decl *EnumerationType::getDeclaration()
{
    return correspondingDecl;
}

//===----------------------------------------------------------------------===//
// IntegerType

IntegerType::IntegerType(const llvm::APInt &low, const llvm::APInt &high)
  : Type(AST_IntegerType), low(low), high(high)
{
    assert(low.getBitWidth() == high.getBitWidth() &&
           "Inconsistent widths for IntegerType bounds!");
}

void IntegerType::Profile(llvm::FoldingSetNodeID &ID,
                          const llvm::APInt &low, const llvm::APInt &high)
{
    low.Profile(ID);
    high.Profile(ID);
}

//===----------------------------------------------------------------------===//
// TypedefType

Decl *TypedefType::getDeclaration()
{
    return declaration;
}
