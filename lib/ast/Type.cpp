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
    return isa<IntegerType>(this);
}

bool Type::isEnumType() const
{
    return isa<EnumerationType>(this);
}

bool Type::isCompositeType() const
{
    // The only kind of composite types ATM are array types.
    return isArrayType();
}

bool Type::isArrayType() const
{
    return isa<ArrayType>(this);
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
    return dyn_cast<ArrayType>(this);
}

IntegerType *Type::getAsIntegerType()
{
    return dyn_cast<IntegerType>(this);
}

EnumerationType *Type::getAsEnumType()
{
    return dyn_cast<EnumerationType>(this);
}

//===----------------------------------------------------------------------===//
// SubroutineType

SubroutineType::SubroutineType(AstKind kind, Type **argTypes, unsigned numArgs)
    : Type(kind),
      argumentTypes(0),
      numArguments(numArgs)
{
    assert(this->denotesSubroutineType());
    if (numArgs > 0) {
        argumentTypes = new Type*[numArgs];
        std::copy(argTypes, argTypes + numArgs, argumentTypes);
    }
}

//===----------------------------------------------------------------------===//
// DomainType

DomainType::DomainType(DomainTypeDecl *DTDecl)
    : PrimaryType(AST_DomainType, 0, false),
      definingDecl(DTDecl)
{ }

DomainType::DomainType(DomainType *rootType, IdentifierInfo *name)
    : PrimaryType(AST_DomainType, rootType, true),
      definingDecl(name)
{ }

IdentifierInfo *DomainType::getIdInfo() const
{
    if (DomainTypeDecl *decl = definingDecl.dyn_cast<DomainTypeDecl*>())
        return decl->getIdInfo();
    return definingDecl.get<IdentifierInfo*>();
}

bool DomainType::involvesPercent() const
{
    if (denotesPercent())
        return true;

    if (const DomainInstanceDecl *instance = getInstanceDecl()) {
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

PrimaryType *DomainType::getRepresentationType()
{
    if (DomainInstanceDecl *decl = getInstanceDecl())
        return decl->getRepresentationType();
    return 0;
}

//===----------------------------------------------------------------------===//
// The following getXXXDecl methods cannot be inlined into Type.h since we do
// not want Type.h to directly depend on Decl.h.

const DomainTypeDecl *DomainType::getDomainTypeDecl() const
{
    const DomainType *root = isSubtype() ? getRootType() : this;
    return root->definingDecl.get<DomainTypeDecl*>();
}

DomainTypeDecl *DomainType::getDomainTypeDecl()
{
    DomainType *root = isSubtype() ? getRootType() : this;
    return root->definingDecl.get<DomainTypeDecl*>();
}

const PercentDecl *DomainType::getPercentDecl() const
{
    return dyn_cast<PercentDecl>(getDomainTypeDecl());
}

PercentDecl *DomainType::getPercentDecl()
{
    return dyn_cast<PercentDecl>(getDomainTypeDecl());
}

const DomainInstanceDecl *DomainType::getInstanceDecl() const
{
    return dyn_cast<DomainInstanceDecl>(getDomainTypeDecl());
}

DomainInstanceDecl *DomainType::getInstanceDecl()
{
    return dyn_cast<DomainInstanceDecl>(getDomainTypeDecl());
}

const AbstractDomainDecl *DomainType::getAbstractDecl() const
{
    return dyn_cast<AbstractDomainDecl>(getDomainTypeDecl());
}

AbstractDomainDecl *DomainType::getAbstractDecl()
{
    return dyn_cast<AbstractDomainDecl>(getDomainTypeDecl());
}

//===----------------------------------------------------------------------===//
// DiscreteType

DiscreteType::ContainmentResult
DiscreteType::contains(const DiscreteType *target) const
{
    // All types trivially contain themselves.
    if (this == target)
        return Is_Contained;

    // Check that the categories of both types match.
    if (this->getKind() != target->getKind())
        return Not_Contained;

    // Obtain the bounds for this type.
    llvm::APInt min;
    llvm::APInt max;

    if (const RangeConstraint *constraint = getConstraint()) {
        // If this type has a non-static constraint we cannot compute
        // containment.
        if (!constraint->isStatic())
            return Maybe_Contained;

        // If this type has a null range, we cannot contain the target.
        if (constraint->isNull())
            return Not_Contained;

        min = constraint->getStaticLowerBound();
        max = constraint->getStaticUpperBound();
    }
    else {
        // Use the representational limits.
        getLowerLimit(min);
        getUpperLimit(max);
    }


    // Obtain bounds for the target.
    llvm::APInt lower;
    llvm::APInt upper;

    if (const RangeConstraint *constraint = target->getConstraint()) {
        if (constraint->isStatic()) {
            // If the target is constrained to a null range, we can always
            // contain it.
            if (constraint->isNull())
                return Is_Contained;

            lower = constraint->getStaticLowerBound();
            upper = constraint->getStaticUpperBound();
        }
        else {
            // For dynamic constrains use the representational limits.
            target->getLowerLimit(lower);
            target->getUpperLimit(upper);
        }
    }
    else {
        // Use the representational limits.
        target->getLowerLimit(lower);
        target->getUpperLimit(upper);
    }

    // Adjust the collected bounds so that they are of equal widths and compare.
    unsigned width = std::max(getSize(), target->getSize());
    min.sextOrTrunc(width);
    max.sextOrTrunc(width);
    lower.sextOrTrunc(width);
    upper.sextOrTrunc(width);
    if (min.sle(lower) && upper.sle(max))
        return Is_Contained;
    else
        return Not_Contained;
}

DiscreteType::ContainmentResult
DiscreteType::contains(const llvm::APInt &value) const
{
    ContainmentResult result = Not_Contained;

    if (const Range *constraint = getConstraint()) {
        if (constraint->isStatic())
            result = constraint->contains(value) ? Is_Contained : Not_Contained;
        else
            result = Maybe_Contained;
    }
    else {
        llvm::APInt lower;
        llvm::APInt upper;
        llvm::APInt candidate(value);

        getLowerLimit(lower);
        getUpperLimit(upper);

        // Adjust the collected values so that they are of equal widths and
        // compare.
        unsigned width = std::max(getSize(), uint64_t(value.getBitWidth()));
        if (isSigned()) {
            lower.sextOrTrunc(width);
            upper.sextOrTrunc(width);
            candidate.sextOrTrunc(width);
            result = lower.sle(candidate) && candidate.sle(upper)
                ? Is_Contained : Not_Contained;
        }
        else {
            lower.zextOrTrunc(width);
            upper.zextOrTrunc(width);
            candidate.zextOrTrunc(width);
            result = lower.ule(candidate) && candidate.ule(upper)
                ? Is_Contained : Not_Contained;
        }
    }

    return result;
}

unsigned DiscreteType::getPreferredSize(uint64_t bits)
{
    unsigned size;

    if (bits <= 1)
        size = 1;
    else if (bits <= 8)
        size = 8;
    else if (bits <= 16)
        size = 16;
    else if (bits <= 32)
        size = 32;
    else if (bits <= 64)
        size = 64;
    else {
        assert(false && "Range too wide to represent!");
        size = 64;
    }
    return size;
}

bool DiscreteType::isSigned() const
{
    // Integer types are signed but enumerations are not.
    return isa<IntegerType>(this);
}

uint64_t DiscreteType::length() const
{
    if (const Range *range = getConstraint())
        return range->length();

    // FIXME: This code is a close duplicate of Range::length.  This should be
    // shared.
    int64_t lower;
    int64_t upper;

    // Extract the bounds of this range according to the type.
    if (this->isSigned()) {
        llvm::APInt limit;
        getLowerLimit(limit);
        lower = limit.getSExtValue();
        getUpperLimit(limit);
        upper = limit.getSExtValue();
    }
    else {
        llvm::APInt limit;
        getLowerLimit(limit);
        lower = limit.getZExtValue();
        getUpperLimit(limit);
        upper = limit.getZExtValue();
    }

    if (upper < lower)
        return 0;

    if (lower < 0) {
        uint64_t lowElems = -lower;
        if (upper >= 0)
            return uint64_t(upper) + lowElems + 1;
        else {
            uint64_t upElems = -upper;
            return lowElems - upElems + 1;
        }
    }

    return uint64_t(upper - lower) + 1;
}

//===----------------------------------------------------------------------===//
// EnumerationType
//
// EnumerationType nodes are implemented using one of three classes
// corresponding to the three fundamental types of enumeration nodes: root,
// constrained subtypes, and unconstrained subtype.

namespace {

class UnconstrainedEnumType;
class ConstrainedEnumType;

class RootEnumType : public EnumerationType {

public:
    RootEnumType(AstResource &resource, EnumerationDecl *decl);

    /// Returns the defining identifier associated with this integer type.
    IdentifierInfo *getIdInfo() const { return definingDecl->getIdInfo(); }

    //@{
    /// Returns the declaration node defining this type.
    EnumerationDecl *getDefiningDecl() { return definingDecl; }
    const EnumerationDecl *getDefiningDecl() const { return definingDecl; }
    //@}

    /// Returns the number of literals supported by this type.
    uint64_t getNumLiterals() const { return definingDecl->getNumLiterals(); }

    /// Returns the number of bits needed to represent this type.
    ///
    /// For root enumeration types the size is either 8, 16, 32, or 64.
    uint64_t getSize() const;

    //@{
    /// Returns the unconstrained base subtype.
    const EnumerationType *getBaseSubtype() const;
    EnumerationType *getBaseSubtype();
    //@}

    /// Returns the lower limit of this type.
    void getLowerLimit(llvm::APInt &res) const {
        res = llvm::APInt(getSize(), uint64_t(0), true);
    }

    /// Returns the upper limit of this type.
    void getUpperLimit(llvm::APInt &res) const {
        uint64_t max = definingDecl->getNumLiterals() - 1;
        res = llvm::APInt(getSize(), max, true);
    }

    // Support isa/dyn_cast.
    static bool classof(const RootEnumType *node) { return true; }
    static bool classof(const EnumerationType *node) {
        return node->getEnumKind() == RootEnumType_KIND;
    }

private:
    UnconstrainedEnumType *baseType; ///< Base subtype.
    EnumerationDecl *definingDecl;   ///< Underlying declaration.
};

class UnconstrainedEnumType : public EnumerationType {

public:
    UnconstrainedEnumType(RootEnumType *rootType,
                          IdentifierInfo *name = 0)
        : EnumerationType(UnconstrainedEnumType_KIND, rootType),
          idInfo(name) { }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return idInfo == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this subtype is anonymous, the identifier returned is that of the
    /// root type.
    IdentifierInfo *getIdInfo() const {
        return idInfo ? idInfo : getRootType()->getIdInfo();
    }

    // Support isa/dyn_cast.
    static bool classof(const UnconstrainedEnumType *node) { return true; }
    static bool classof(const EnumerationType *node) {
        return node->getEnumKind() == UnconstrainedEnumType_KIND;
    }

private:
    IdentifierInfo *idInfo;
};

class ConstrainedEnumType : public EnumerationType {

public:
    ConstrainedEnumType(RootEnumType *rootType,
                        Expr *lowerBound, Expr *upperBound,
                        IdentifierInfo *name = 0)
        : EnumerationType(ConstrainedEnumType_KIND, rootType),
          constraint(new RangeConstraint(lowerBound, upperBound, rootType)),
          idInfo(name) { }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return idInfo == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this is an anonymous type, returns the identifier of the underlying
    /// root type.
    IdentifierInfo *getIdInfo() const {
        return idInfo ? idInfo : getRootType()->getIdInfo();
    }

    //@{
    /// Returns the constraint on this subtype.
    RangeConstraint *getConstraint() { return constraint; }
    const RangeConstraint *getConstraint() const { return constraint; }
    //@}

    /// \brief Returns true if the constraint bounds for this subtype are
    /// statically known.
    bool isStaticallyConstrained() const { return constraint->isStatic(); }

    // Support isa/dyn_cast.
    static bool classof(const ConstrainedEnumType *node) { return true; }
    static bool classof(const EnumerationType *node) {
        return node->getEnumKind() == ConstrainedEnumType_KIND;
    }

private:
    RangeConstraint *constraint; ///< The constraint on this subtype.
    IdentifierInfo *idInfo;      ///< The name of this subtype or null.
};

} // end anonymous namespace.

RootEnumType::RootEnumType(AstResource &resource, EnumerationDecl *decl)
    : EnumerationType(RootEnumType_KIND, 0),
      definingDecl(decl)
{
    // Build the base unconstrained subtype node.  This type is given the name
    // "I'Base" where I is the defining identifier of the corresponding
    // declaration.
    llvm::Twine name(definingDecl->getString());
    name = name + "'Base";
    IdentifierInfo *id = resource.getIdentifierInfo(name.str());
    baseType = cast<UnconstrainedEnumType>(
        resource.createEnumSubtype(id, this));
}

uint64_t RootEnumType::getSize() const
{
    uint64_t numBits = llvm::Log2_64_Ceil(definingDecl->getNumLiterals());
    return DiscreteType::getPreferredSize(numBits);
}

const EnumerationType *RootEnumType::getBaseSubtype() const
{
    return baseType;
}

EnumerationType *RootEnumType::getBaseSubtype()
{
    return baseType;
}

const EnumerationDecl *EnumerationType::getDeclaration() const
{
    const RootEnumType *root = cast<RootEnumType>(getRootType());
    return root->getDefiningDecl();
}

bool EnumerationType::isCharacterType() const
{
    return getDeclaration()->isCharacterType();
};

uint64_t EnumerationType::getNumLiterals() const
{
    return cast<RootEnumType>(getRootType())->getNumLiterals();
}

RangeConstraint *EnumerationType::getConstraint()
{
    if (ConstrainedEnumType *Ty = dyn_cast<ConstrainedEnumType>(this))
        return Ty->getConstraint();
    return 0;
}

const RangeConstraint *EnumerationType::getConstraint() const
{
    if (const ConstrainedEnumType *Ty = dyn_cast<ConstrainedEnumType>(this))
        return Ty->getConstraint();
    return 0;
}

void EnumerationType::getLowerLimit(llvm::APInt &res) const
{
    cast<RootEnumType>(getRootType())->getLowerLimit(res);
}

void EnumerationType::getUpperLimit(llvm::APInt &res) const
{
    cast<RootEnumType>(this->getRootType())->getUpperLimit(res);
}

uint64_t EnumerationType::getSize() const
{
    return cast<RootEnumType>(this->getRootType())->getSize();
}

const EnumerationType *EnumerationType::getBaseSubtype() const
{
    return const_cast<EnumerationType*>(this)->getBaseSubtype();
}

EnumerationType *EnumerationType::getBaseSubtype()
{
    RootEnumType *root = cast<RootEnumType>(this->getRootType());
    return root->getBaseSubtype();
}

EnumerationType *EnumerationType::create(AstResource &resource,
                                         EnumerationDecl *decl)
{
    return new RootEnumType(resource, decl);
}

EnumerationType *EnumerationType::createSubtype(EnumerationType *type,
                                                IdentifierInfo *name)
{
    RootEnumType *root = cast<RootEnumType>(type->getRootType());
    return new UnconstrainedEnumType(root, name);
}

EnumerationType *
EnumerationType::createConstrainedSubtype(EnumerationType *type,
                                          Expr *lowerBound, Expr *upperBound,
                                          IdentifierInfo *name)
{
    RootEnumType *root = cast<RootEnumType>(type->getRootType());
    return new ConstrainedEnumType(root, lowerBound, upperBound, name);
}

//===----------------------------------------------------------------------===//
// IntegerType
//
// IntegerType nodes are implemented using one of three classes corresponding to
// the three fundamental types of integer nodes:  root, constrained subtype, and
// unconstrained subtype.

namespace {

class ConstrainedIntegerType;
class UnconstrainedIntegerType;

class RootIntegerType : public IntegerType {

public:
    RootIntegerType(AstResource &resource, IntegerDecl *decl,
                    const llvm::APInt &low, const llvm::APInt &high);

    /// Returns the defining identifier associated with this integer type.
    IdentifierInfo *getIdInfo() const { return definingDecl->getIdInfo(); }

    //@{
    /// Returns the declaration node defining this type.
    IntegerDecl *getDefiningDecl() { return definingDecl; }
    const IntegerDecl *getDefiningDecl() const { return definingDecl; }
    //@}

    /// Returns the number of bits to represent this type.
    ///
    /// For root integer types the size is either 8, 16, 32, or 64.
    uint64_t getSize() const { return lowerBound.getBitWidth(); }

    /// Returns the base subtype.
    UnconstrainedIntegerType *getBaseSubtype() { return baseType; }

    /// Returns the lower limit of this type.
    void getLowerLimit(llvm::APInt &res) const { res = lowerBound; }

    /// Returns the upper limit of this type.
    void getUpperLimit(llvm::APInt &res) const { res = upperBound; }

    // Support isa/dyn_cast.
    static bool classof(const RootIntegerType *node) { return true; }
    static bool classof(const IntegerType *node) {
        return node->getIntegerKind() == RootIntegerType_KIND;
    }

private:
    llvm::APInt lowerBound;             ///< Static lower bound for this type.
    llvm::APInt upperBound;             ///< Static upper bound for this type.
    UnconstrainedIntegerType *baseType; ///< Base subtype.
    IntegerDecl *definingDecl;          ///< Defining declaration node.

    /// Constructor helper.  Initializes the lower and upper bounds.
    void initBounds(const llvm::APInt &low, const llvm::APInt &high);
};

class ConstrainedIntegerType : public IntegerType {

public:
    ConstrainedIntegerType(RootIntegerType *rootType,
                           Expr *lowerBound, Expr *upperBound,
                           IdentifierInfo *name = 0)
        : IntegerType(ConstrainedIntegerType_KIND, rootType),
          constraint(new RangeConstraint(lowerBound, upperBound, rootType)),
          idInfo(name) { }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return idInfo == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this is an anonymous subtype, the identifier returned is that of the
    /// root type.
    IdentifierInfo *getIdInfo() const {
        return idInfo ? idInfo : getRootType()->getIdInfo();
    }

    //@{
    /// Returns the constraint on this subtype.
    RangeConstraint *getConstraint() { return constraint; }
    const RangeConstraint *getConstraint() const { return constraint; }
    //@}

    /// \brief Returns true if the constraint bounds for this subtype are
    /// statically constrained.
    bool isStaticallyConstrained() const { return constraint->isStatic(); }

    // Support isa/dyn_cast.
    static bool classof(const ConstrainedIntegerType *node) { return true; }
    static bool classof(const IntegerType *node) {
        return node->getIntegerKind() == ConstrainedIntegerType_KIND;
    }

private:
    RangeConstraint *constraint; ///< The constraints on this subtype.
    IdentifierInfo *idInfo;      ///< The name of this subtype or null.
};

class UnconstrainedIntegerType : public IntegerType {

public:
    UnconstrainedIntegerType(RootIntegerType *rootType,
                             IdentifierInfo *name = 0)
        : IntegerType(UnconstrainedIntegerType_KIND, rootType),
          idInfo(name) { }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return idInfo == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this is an anonymous subtype, the identifier returned is that of the
    /// root type.
    IdentifierInfo *getIdInfo() const {
        return idInfo ? idInfo : getRootType()->getIdInfo();
    }

    // Support isa/dyn_cast.
    static bool classof(const UnconstrainedIntegerType *node) { return true; }
    static bool classof(const IntegerType *node) {
        return node->getIntegerKind() == UnconstrainedIntegerType_KIND;
    }

private:
    IdentifierInfo *idInfo;     ///< The name of this subtype or null.
};

} // end anonymous namespace.

RootIntegerType::RootIntegerType(AstResource &resource,
                                 IntegerDecl *decl,
                                 const llvm::APInt &low,
                                 const llvm::APInt &high)
    : IntegerType(RootIntegerType_KIND, 0),
      definingDecl(decl)
{
    initBounds(low, high);

    // Build the base unconstrained subtype node.  This type is given the name
    // "I'Base", where I is the defining identifier of the corresponding
    // declaration.
    llvm::Twine name(definingDecl->getString());
    name = name + "'Base";
    IdentifierInfo *id = resource.getIdentifierInfo(name.str());
    baseType = cast<UnconstrainedIntegerType>(
        resource.createIntegerSubtype(id, this));
}

void RootIntegerType::initBounds(const llvm::APInt &low,
                                 const llvm::APInt &high)
{
    // The base range represents a two's-complement signed integer.  We must be
    // symmetric about zero and include the values of the bounds.  Therefore,
    // even for null ranges, our base range is at least 2**7-1 .. 2**7.
    unsigned minimalWidth = std::max(low.getMinSignedBits(),
                                     high.getMinSignedBits());

    unsigned preferredWidth = DiscreteType::getPreferredSize(minimalWidth);
    lowerBound = llvm::APInt::getSignedMinValue(preferredWidth);
    upperBound = llvm::APInt::getSignedMaxValue(preferredWidth);
}

IntegerType *IntegerType::create(AstResource &resource, IntegerDecl *decl,
                                 const llvm::APInt &lower,
                                 const llvm::APInt &upper)
{
    return new RootIntegerType(resource, decl, lower, upper);
}

IntegerType *IntegerType::createSubtype(IntegerType *type,
                                        IdentifierInfo *name)
{
    RootIntegerType *root = cast<RootIntegerType>(type->getRootType());
    return new UnconstrainedIntegerType(root, name);
}

IntegerType *IntegerType::createConstrainedSubtype(IntegerType *type,
                                                   Expr *lowerBound,
                                                   Expr *upperBound,
                                                   IdentifierInfo *name)
{
    RootIntegerType *root = cast<RootIntegerType>(type->getRootType());
    return new ConstrainedIntegerType(root, lowerBound, upperBound, name);
}

IntegerType *IntegerType::getBaseSubtype()
{
    RootIntegerType *root = cast<RootIntegerType>(this->getRootType());
    return root->getBaseSubtype();
}

RangeConstraint *IntegerType::getConstraint()
{
    ConstrainedIntegerType *Ty;
    Ty = dyn_cast<ConstrainedIntegerType>(this);
    return Ty ? Ty->getConstraint() : 0;
}

const RangeConstraint *IntegerType::getConstraint() const {
    const ConstrainedIntegerType *Ty;
    Ty = dyn_cast<ConstrainedIntegerType>(this);
    return Ty ? Ty->getConstraint() : 0;
}

void IntegerType::getLowerLimit(llvm::APInt &res) const
{
    const RootIntegerType *root = cast<RootIntegerType>(getRootType());
    root->getLowerLimit(res);
}

void IntegerType::getUpperLimit(llvm::APInt &res) const
{
    const RootIntegerType *root = cast<RootIntegerType>(getRootType());
    root->getUpperLimit(res);
}

bool IntegerType::baseContains(const llvm::APInt &value) const
{
    uint64_t activeBits = value.getMinSignedBits();
    uint64_t width = getSize();

    if (activeBits > width)
        return false;

    llvm::APInt lower;
    llvm::APInt upper;
    getLowerLimit(lower);
    getUpperLimit(upper);

    // If this type was specified with a null range we cannot contain any
    // values.
    if (lower.sgt(upper))
        return false;

    // Compute a copy of the given value and sign extended to the width of the
    // base type if needed.
    llvm::APInt candidate(value);
    if (activeBits < width)
        candidate.sext(width);

    return lower.sle(candidate) && candidate.sle(upper);
}

uint64_t IntegerType::getSize() const
{
    const RootIntegerType *root = cast<RootIntegerType>(getRootType());
    return root->getSize();
}

//===----------------------------------------------------------------------===//
// ArrayType

ArrayType::ArrayType(ArrayDecl *decl, unsigned rank, DiscreteType **indices,
                     Type *component, bool isConstrained)
    : PrimaryType(AST_ArrayType, 0, false),
      constraint(indices, indices + rank),
      componentType(component),
      definingDecl(decl)
{
    assert(rank != 0 && "Missing index types!");
    assert(this->isRootType());

    if (isConstrained)
        setConstraintBit();
}

ArrayType::ArrayType(IdentifierInfo *name, ArrayType *rootType,
                     DiscreteType **indices)
    : PrimaryType(AST_ArrayType, rootType, true),
      constraint(indices, indices + rootType->getRank()),
      componentType(rootType->getComponentType()),
      definingDecl(name)
{
    setConstraintBit();
    assert(this->isSubtype());
}

ArrayType::ArrayType(IdentifierInfo *name, ArrayType *rootType)
    : PrimaryType(AST_ArrayType, rootType, true),
      constraint(rootType->constraint),
      componentType(rootType->getComponentType()),
      definingDecl(name)
{
    assert(this->isSubtype());
    if (getRootType()->isConstrained())
        setConstraintBit();
}

IdentifierInfo *ArrayType::getIdInfo() const
{
    if (IdentifierInfo *idInfo = definingDecl.dyn_cast<IdentifierInfo*>())
        return idInfo;
    const ArrayType *root = getRootType();
    return root->definingDecl.get<ArrayDecl*>()->getIdInfo();
}

uint64_t ArrayType::length() const
{
    assert(isConstrained() &&
           "Cannot determine length of unconstrained arrays!");

    const DiscreteType *indexTy = getIndexType(0);
    const RangeConstraint *constraint = indexTy->getConstraint();

    assert(constraint->isStatic() &&
           "Cannot determine length using non-static index constraint!");

    if (constraint->isNull())
        return 0;

    /// FIXME: There is an problem with overflow here when the bounds are at the
    /// limit for a signed 64 bit integer.  One solution is to have this method
    /// return an APInt.  Another is to make the length undefined for arrays
    /// with a null index and return a "zero based" result.
    llvm::APInt lower(constraint->getStaticLowerBound());
    llvm::APInt upper(constraint->getStaticUpperBound());
    llvm::APInt length(upper - lower);
    return length.getZExtValue() + 1;
}

