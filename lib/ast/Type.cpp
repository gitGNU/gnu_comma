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
