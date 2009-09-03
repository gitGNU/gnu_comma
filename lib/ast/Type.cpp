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

bool Type::equals(const Type *type) const
{
    return type == this;
}

bool Type::isScalarType() const
{
    if (const CarrierType *carrier = dyn_cast<CarrierType>(this))
        return carrier->getRepresentationType()->isScalarType();

    if (const TypedefType *TyDef = dyn_cast<TypedefType>(this))
        return TyDef->getBaseType()->isScalarType();

    return isa<EnumerationType>(this) or isIntegerType();
}

bool Type::isIntegerType() const
{
    if (const CarrierType *carrier = dyn_cast<CarrierType>(this))
        return carrier->getRepresentationType()->isIntegerType();

    if (const TypedefType *TyDef = dyn_cast<TypedefType>(this))
        return TyDef->getBaseType()->isIntegerType();

    return isa<IntegerType>(this);
}


//===----------------------------------------------------------------------===//
// CarrierType

CarrierType::CarrierType(CarrierDecl *carrier)
  : NamedType(AST_CarrierType, carrier->getIdInfo()),
    declaration(carrier) { }

CarrierDecl *CarrierType::getDeclaration()
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

bool CarrierType::equals(const Type *type) const
{
    if (this == type) return true;
    return getRepresentationType()->equals(type);
}

//===----------------------------------------------------------------------===//
// SignatureType

SignatureType::SignatureType(SignatureDecl *decl)
    : NamedType(AST_SignatureType, decl->getIdInfo()),
      declaration(decl)
{ }

SignatureType::SignatureType(VarietyDecl *decl,
                             Type **args, unsigned numArgs)
    : NamedType(AST_SignatureType, decl->getIdInfo()),
      declaration(decl)
{
    arguments = new Type*[numArgs];
    std::copy(args, args + numArgs, arguments);
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
// DomainType

DomainType::DomainType(DomainTypeDecl *DTDecl)
    : NamedType(AST_DomainType, DTDecl->getIdInfo()),
      declaration(DTDecl)
{ }

DomainType::DomainType(IdentifierInfo *percentId, ModelDecl *model)
    : NamedType(AST_DomainType, percentId),
      declaration(model)
{ }

bool DomainType::denotesPercent() const
{
    // When our defining declaration is a model, this type represents the % node
    // of that model.
    return isa<ModelDecl>(declaration);
}

bool DomainType::involvesPercent() const
{
    if (denotesPercent())
        return true;

    if (DomainInstanceDecl *instance = getInstanceDecl()) {
        unsigned arity = instance->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            DomainType *param = dyn_cast<DomainType>(
                instance->getActualParameter(i));
            if (param && param->involvesPercent())
                return true;
        }
    }
    return false;
}

ModelDecl *DomainType::getModelDecl() const
{
    return dyn_cast<ModelDecl>(declaration);
}

DomainTypeDecl *DomainType::getDomainTypeDecl() const
{
    return dyn_cast<DomainTypeDecl>(declaration);
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

//===----------------------------------------------------------------------===//
// EnumerationType

EnumerationType::EnumerationType(EnumerationDecl *decl)
    : NamedType(AST_EnumerationType, decl->getIdInfo()),
      correspondingDecl(decl) { }

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

TypedefType::TypedefType(Type *baseType, Decl *decl)
    : NamedType(AST_TypedefType, decl->getIdInfo()),
      baseType(baseType),
      declaration(decl) { }

Decl *TypedefType::getDeclaration()
{
    return declaration;
}
