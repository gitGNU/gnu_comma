//===-- ast/Type.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/Twine.h"

#include <algorithm>
#include <iostream>

using namespace comma;
using llvm::cast;
using llvm::dyn_cast;
using llvm::isa;

//===----------------------------------------------------------------------===//
// Type

bool Type::memberOf(Classification ID) const
{
    switch (ID) {
    default:
        assert(false && "Bad classification ID!");
        return false;
    case CLASS_Scalar:
        return isScalarType();
    case CLASS_Discrete:
        return isDiscreteType();
    case CLASS_Enum:
        return isEnumType();
    case CLASS_Integer:
        return isIntegerType();
    case CLASS_Composite:
        return isCompositeType();
    case CLASS_Array:
        return isArrayType();
    case CLASS_String:
        return isStringType();
    }
}

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

bool Type::isCompositeType() const
{
    // The only kind of composite types ATM are array types.
    return isArrayType();
}

bool Type::isArrayType() const
{
    return isa<ArraySubType>(this) || isa<ArrayType>(this);
}

bool Type::isStringType() const
{
    const ArrayType *arrTy = const_cast<Type*>(this)->getAsArrayType();
    if (!arrTy || !arrTy->isVector())
        return false;

    EnumerationType *enumTy = arrTy->getComponentType()->getAsEnumType();
    return enumTy && enumTy->isCharacterType();
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

IntegerSubType *Type::getAsIntegerSubType()
{
    if (IntegerSubType *subtype = dyn_cast<IntegerSubType>(this))
        return subtype;
    if (CarrierType *carrier = dyn_cast<CarrierType>(this))
        return carrier->getParentType()->getAsIntegerSubType();
    return 0;
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

IntegerType::IntegerType(AstResource &resource, IntegerDecl *decl,
                         const llvm::APInt &low, const llvm::APInt &high)
    : Type(AST_IntegerType)
{
    initBounds(low, high);

    // Build the base unconstrained subtype node named "I'Base", where I is the
    // defining identifier for the corresponding declaration.
    llvm::Twine name(decl->getString());
    name = name + "'Base";
    IdentifierInfo *id = resource.getIdentifierInfo(name.str());
    baseSubType = resource.createIntegerSubType(id, this);
}

unsigned IntegerType::getWidthForRange(const llvm::APInt &low,
                                       const llvm::APInt &high)
{
     return std::max(low.getMinSignedBits(), high.getMinSignedBits());
}

void IntegerType::initBounds(const llvm::APInt &low, const llvm::APInt &high)
{
    // The base range represents a two's-complement signed integer.  We must be
    // symmetric about zero and include the values of the bounds.  Therefore,
    // even for null ranges, our base range is at least 2**7-1 .. 2**7.
    unsigned minimalWidth = getWidthForRange(low, high);
    unsigned preferredWidth;

    if (minimalWidth <= 8)
        preferredWidth = 8;
    else if (minimalWidth <= 16)
        preferredWidth = 16;
    else if (minimalWidth <= 32)
        preferredWidth = 32;
    else if (minimalWidth <= 64)
        preferredWidth = 64;
    else {
        assert(false && "Range too wide to represent!");
        preferredWidth = 64;
    }

    this->low = llvm::APInt::getSignedMinValue(preferredWidth);
    this->high = llvm::APInt::getSignedMaxValue(preferredWidth);
}

bool IntegerType::contains(IntegerSubType *subtype) const
{
    // If the given subtype is unconstrained, check if this type contains its
    // base.
    if (!subtype->isConstrained())
        return contains(subtype->getTypeOf());

    // Otherwise, the range of the subtype must be within the representational
    // limits for this type.
    RangeConstraint *range = subtype->getConstraint();
    int64_t target;
    int64_t source;

    // If the target range is null, we always contain such a type.
    if (range->isNull())
        return true;

    // Check if the lower bounds are in range.
    target = low.getSExtValue();
    source = range->getLowerBound().getSExtValue();
    if (source < target)
        return false;

    // Likewise for the upper bounds.
    target = high.getSExtValue();
    source = range->getUpperBound().getSExtValue();
    if (source > target)
        return false;

    return true;
}

//===----------------------------------------------------------------------===//
// ArrayType

ArrayType::ArrayType(unsigned rank, SubType **indices, Type *component,
                     bool isConstrained)
    : Type(AST_ArrayType),
      rank(rank),
      componentType(component)
{
    assert(rank != 0 && "Missing index types!");

    // Use the bits field to record our status as a constrained type.
    if (isConstrained)
        bits |= CONSTRAINT_BIT;

    // Build our own vector of index types.
    indexTypes = new SubType*[rank];
    std::copy(indices, indices + rank, indexTypes);
}

uint64_t ArrayType::length() const
{
    assert(isConstrained() &&
           "Cannot determine length of unconstrained arrays!");

    SubType *indexTy = getIndexType(0);

    if (IntegerSubType *intTy = dyn_cast<IntegerSubType>(indexTy)) {
        llvm::APInt lower;
        llvm::APInt upper;
        if (intTy->isConstrained()) {
            RangeConstraint *range = intTy->getConstraint();
            if (range->isNull())
                return 0;
            lower = range->getLowerBound();
            upper = range->getUpperBound();
        }
        else {
            IntegerType *base = intTy->getTypeOf();
            lower = base->getLowerBound();
            upper = base->getUpperBound();
        }

        llvm::APInt length(upper - lower);
        ++length;
        return length.getZExtValue();
    }

    // FIXME: We do not support constrained enumeration types yet, so just use
    // the number of elements in the base type.
    EnumSubType *enumTy = cast<EnumSubType>(indexTy);
    return enumTy->getTypeOf()->getNumElements();
}

//===----------------------------------------------------------------------===//
// IntegerSubType

IntegerSubType::IntegerSubType(IdentifierInfo *name, IntegerType *type,
                               const llvm::APInt &low, const llvm::APInt &high)
    : SubType(AST_IntegerSubType, name, type, 0)
{
    unsigned width = type->getBitWidth();
    llvm::APInt lower(low);
    llvm::APInt upper(high);

    // We need to ensure that the width of the bounds is the same as the width
    // of the base type.  Ensure that we are not loosing any significant bits.
    assert(low.getMinSignedBits() <= width && "Bounds too wide!");
    assert(high.getMinSignedBits() <= width && "Bounds too wide!");

    lower.sextOrTrunc(width);
    upper.sextOrTrunc(width);

    SubTypeConstraint = new RangeConstraint(lower, upper);

    // FIXME: Add this to the base types list of subtypes.
}


bool IntegerSubType::contains(IntegerSubType *subtype) const
{
    uint64_t lowerTarget;
    uint64_t upperTarget;
    uint64_t lowerSource;
    uint64_t upperSource;

    // If this subtype has a null constraint it cannot contain any other type.
    if (isConstrained() && getConstraint()->isNull())
        return false;

    // If the target subtype is null, we certainly can contain it.
    if (subtype->isConstrained() && subtype->getConstraint()->isNull())
        return true;

    // Otherwise, obtain bounds.
    if (RangeConstraint *range = subtype->getConstraint()) {
        lowerTarget = range->getLowerBound().getSExtValue();
        upperTarget = range->getUpperBound().getSExtValue();
    }
    else {
        IntegerType *baseTy = subtype->getTypeOf();
        lowerTarget = baseTy->getLowerBound().getSExtValue();
        upperTarget = baseTy->getUpperBound().getSExtValue();
    }

    if (RangeConstraint *range = this->getConstraint()) {
        lowerSource = range->getLowerBound().getSExtValue();
        upperSource = range->getUpperBound().getSExtValue();
    }
    else {
        IntegerType *baseTy = this->getTypeOf();
        lowerSource = baseTy->getLowerBound().getSExtValue();
        upperSource = baseTy->getUpperBound().getSExtValue();
    }

    if ((lowerTarget < lowerSource) || (upperSource < upperTarget))
        return false;
    return true;
}

bool IntegerSubType::contains(IntegerType *type) const
{
    // If this subtype is unconstrained, check if the base type contains the
    // target. Otherwise, compare the bounds of this subtypes range and the
    // given types representation limits.
    if (!isConstrained())
        return getTypeOf()->contains(type);

    uint64_t lowerTarget = type->getLowerBound().getSExtValue();
    uint64_t upperTarget = type->getUpperBound().getSExtValue();

    RangeConstraint *range = this->getConstraint();
    uint64_t lowerSource = range->getLowerBound().getSExtValue();
    uint64_t upperSource = range->getUpperBound().getSExtValue();

    if ((lowerTarget < lowerSource) || (upperSource < upperTarget))
        return false;
    return true;
}
