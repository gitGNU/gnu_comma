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
    // Currently, a scalar type is always discrete.
    return isDiscreteType();
}

bool Type::isDiscreteType() const
{
    return isIntegerType() || isEnumType();
}

bool Type::isIntegerType() const
{
    return isa<IntegerSubType>(this) || isa<IntegerType>(this);
}

bool Type::isEnumType() const
{
    return isa<EnumSubType>(this) || isa<EnumerationType>(this);
}

bool Type::isArrayType() const
{
    return isa<ArraySubType>(this) || isa<ArrayType>(this);
}

ArrayType *Type::getAsArrayType()
{
    if (ArraySubType *subtype = dyn_cast<ArraySubType>(this))
        return subtype->getTypeOf();

    return dyn_cast<ArrayType>(this);
}

IntegerType *Type::getAsIntegerType()
{
    if (IntegerSubType *subtype = dyn_cast<IntegerSubType>(this))
        return subtype->getTypeOf();

    return dyn_cast<IntegerType>(this);
}

EnumerationType *Type::getAsEnumType()
{
    if (EnumSubType *subtype = dyn_cast<EnumSubType>(this))
        return subtype->getTypeOf();

    return dyn_cast<EnumerationType>(this);
}

//===----------------------------------------------------------------------===//
// SubType

SubType::SubType(AstKind kind, IdentifierInfo *identifier, Type *parent,
                 Constraint *constraint)
    : Type(kind),
      DefiningIdentifier(identifier),
      ParentType(parent),
      SubTypeConstraint(constraint)
{
    assert(this->denotesSubType());
}

SubType::SubType(AstKind kind, Type *parent, Constraint *constraint)
    : Type(kind),
      DefiningIdentifier(0),
      ParentType(parent),
      SubTypeConstraint(constraint)
{
    assert(this->denotesSubType());
}

Type *SubType::getTypeOf() const
{
    Type *type = ParentType;
    while (SubType *subtype = dyn_cast<SubType>(type)) {
        type = subtype->getParentType();
    }
    return type;
}

//===----------------------------------------------------------------------===//
// CarrierType

CarrierType::CarrierType(CarrierDecl *carrier, Type *type)
    : SubType(AST_CarrierType, carrier->getIdInfo(), type, 0),
      declaration(carrier) { }

IdentifierInfo *CarrierType::getIdInfo() const
{
    return declaration->getIdInfo();
}

//===----------------------------------------------------------------------===//
// DomainType

DomainType::DomainType(DomainTypeDecl *DTDecl)
    : Type(AST_DomainType),
      declaration(DTDecl)
{ }

IdentifierInfo *DomainType::getIdInfo() const
{
    return declaration->getIdInfo();
}

bool DomainType::involvesPercent() const
{
    if (denotesPercent())
        return true;

    if (DomainInstanceDecl *instance = getInstanceDecl()) {
        unsigned arity = instance->getArity();
        for (unsigned i = 0; i < arity; ++i) {
            DomainType *param = dyn_cast<DomainType>(
                instance->getActualParamType(i));
            if (param && param->involvesPercent())
                return true;
        }
    }
    return false;
}

DomainTypeDecl *DomainType::getDomainTypeDecl() const
{
    return dyn_cast<DomainTypeDecl>(declaration);
}

PercentDecl *DomainType::getPercentDecl() const
{
    return dyn_cast<PercentDecl>(declaration);
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
// IntegerType

IntegerType::IntegerType(IntegerDecl *decl,
                         const llvm::APInt &low, const llvm::APInt &high)
    : Type(AST_IntegerType),
      declaration(decl)
{
    // Initialize the bounds for this type.
    std::pair<llvm::APInt, llvm::APInt> bounds = getBaseRange(low, high);
    this->low = bounds.first;
    this->high = bounds.second;

    // The base range my be larger or smaller that the bit width of the given
    // bounds (since the bounds can be computed using any integer type).  Sign
    // extend or truncate as needed -- will will not loose any bits.
    llvm::APInt lowBound(low);
    llvm::APInt highBound(high);
    unsigned width = getBitWidth();
    lowBound.sextOrTrunc(width);
    highBound.sextOrTrunc(width);

    RangeConstraint *constraint = new RangeConstraint(lowBound, highBound);
    FirstSubType = new IntegerSubType(decl->getIdInfo(), this, constraint);

    // Create an anonymous, unconstrained base subtype.
    //
    // FIXME: It may be reasonable to name this subtype "S'Base", where S is
    // this types defining identifier.  We would need to have AstResource pass a
    // pointer to itself so that we have access to the identifier pool.
    BaseSubType = new IntegerSubType(this, 0);
}

unsigned IntegerType::getWidthForRange(const llvm::APInt &low,
                                       const llvm::APInt &high)
{
     return std::max(low.getMinSignedBits(), high.getMinSignedBits());
}

std::pair<llvm::APInt, llvm::APInt>
IntegerType::getBaseRange(const llvm::APInt &low,
                          const llvm::APInt &high)
{
    // The base range represents a two's-complement signed integer.  We must be
    // symmetric about zero and include the values of the bounds.  Therefore,
    // even for null ranges, our base range is at least 2**7-1 .. 2**7.

    unsigned minimalWidth = getWidthForRange(low, high);
    unsigned preferedWidth;

    if (minimalWidth <= 8)
        preferedWidth = 8;
    else if (minimalWidth <= 16)
        preferedWidth = 16;
    else if (minimalWidth <= 32)
        preferedWidth = 32;
    else if (minimalWidth <= 64)
        preferedWidth = 64;
    else {
        assert(false && "Range too wide to represent!");
        preferedWidth = 64;
    }

    llvm::APInt l = llvm::APInt::getSignedMinValue(preferedWidth);
    llvm::APInt h = llvm::APInt::getSignedMaxValue(preferedWidth);
    return std::pair<llvm::APInt, llvm::APInt>(l, h);
}

IdentifierInfo *IntegerType::getIdInfo() const
{
    return declaration->getIdInfo();
}

//===----------------------------------------------------------------------===//
// EnumerationType

EnumerationType::EnumerationType(EnumerationDecl *decl)
    : Type(AST_EnumerationType),
      declaration(decl)
{
    // Create the first unconstrained subtype of this enumeration type.
    FirstSubType = new EnumSubType(decl->getIdInfo(), this, 0);
}

IdentifierInfo *EnumerationType::getIdInfo() const
{
    return declaration->getIdInfo();
}

//===----------------------------------------------------------------------===//
// ArrayType

ArrayType::ArrayType(ArrayDecl *decl,
                     unsigned rank, SubType **indices, Type *component,
                     bool isConstrained)
    : Type(AST_ArrayType),
      rank(rank),
      componentType(component),
      declaration(decl)
{
    assert(rank != 0 && "Missing index types!");

    // Use the bits field to record our status as a constrained type.
    if (isConstrained)
        bits |= CONSTRAINT_BIT;

    // Build our own vector of index types.
    indexTypes = new SubType*[rank];
    std::copy(indices, indices + rank, indexTypes);

    // Create the first subtype of this array type.
    IndexConstraint *constraint = 0;
    if (isConstrained)
        constraint = new IndexConstraint(indices, rank);
    FirstSubType = new ArraySubType(decl->getIdInfo(), this, constraint);
}

IdentifierInfo *ArrayType::getIdInfo() const
{
    return declaration->getIdInfo();
}

