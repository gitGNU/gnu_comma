//===-- ast/Type.cpp ------------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
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
        return isIntegerType() || isUniversalIntegerType();
    case CLASS_Composite:
        return isCompositeType();
    case CLASS_Array:
        return isArrayType();
    case CLASS_String:
        return isStringType();
    case CLASS_Access:
        return isAccessType() || isUniversalAccessType();
    case CLASS_Record:
        return isRecordType();
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
    return this->denotesCompositeType();
}

bool Type::isArrayType() const
{
    return isa<ArrayType>(this);
}

bool Type::isRecordType() const
{
    return isa<RecordType>(this);
}

bool Type::isStringType() const
{
    const ArrayType *arrTy = dyn_cast<ArrayType>(this);
    if (arrTy && arrTy->isVector()) {
        const Type *compTy = arrTy->getComponentType();
        const EnumerationType *enumTy = dyn_cast<EnumerationType>(compTy);
        return enumTy && enumTy->isCharacterType();
    }
    return false;
}

bool Type::isAccessType() const
{
    return isa<AccessType>(this);
}

bool Type::isFatAccessType() const
{
    if (const AccessType *access = dyn_cast<AccessType>(this))
        return access->getTargetType()->isIndefiniteType();
    return false;
}

bool Type::isThinAccessType() const
{
    if (const AccessType *access = dyn_cast<AccessType>(this))
        return !access->getTargetType()->isIndefiniteType();
    return false;
}

bool Type::isUniversalType() const
{
    return isa<UniversalType>(this);
}

bool Type::isUniversalIntegerType() const
{
    if (const UniversalType *univTy = dyn_cast<UniversalType>(this))
        return univTy->isUniversalIntegerType();
    return false;
}

bool Type::isUniversalAccessType() const
{
    if (const UniversalType *univTy = dyn_cast<UniversalType>(this))
        return univTy->isUniversalAccessType();
    return false;
}

bool Type::isUniversalFixedType() const
{
    if (const UniversalType *univTy = dyn_cast<UniversalType>(this))
        return univTy->isUniversalFixedType();
    return false;
}

bool Type::isUniversalRealType() const
{
    if (const UniversalType *univTy = dyn_cast<UniversalType>(this))
        return univTy->isUniversalRealType();
    return false;
}

bool Type::isUniversalTypeOf(const Type *type) const
{
    if (type->isUniversalIntegerType())
        return type->memberOf(CLASS_Integer);
    if (type->isUniversalAccessType())
        return type->memberOf(CLASS_Access);
    if (type->isUniversalFixedType() || type->isUniversalRealType())
        assert(false && "Cannot handle this kind of universal type yet!");

    return false;
}

bool Type::involvesPercent() const
{
    if (const DomainType *domTy = dyn_cast<DomainType>(this)) {
        if (domTy->denotesPercent())
            return true;

        if (const DomainInstanceDecl *instance = domTy->getInstanceDecl()) {
            unsigned arity = instance->getArity();
            for (unsigned i = 0; i < arity; ++i) {
                const DomainType *param = instance->getActualParamType(i);
                if (param->involvesPercent())
                    return true;
            }
        }
        return false;
    }

    if (const ArrayType *arrTy = dyn_cast<ArrayType>(this))
        return arrTy->getComponentType()->involvesPercent();

    if (const SubroutineType *subTy = dyn_cast<SubroutineType>(this)) {
        SubroutineType::arg_type_iterator I = subTy->begin();
        SubroutineType::arg_type_iterator E = subTy->end();
        for ( ; I != E; ++I) {
            const Type *argTy = *I;
            if (argTy->involvesPercent())
                return true;
        }

        if (const FunctionType *fnTy = dyn_cast<FunctionType>(subTy))
            return fnTy->getReturnType()->involvesPercent();
    }

    if (const RecordType *recordTy = dyn_cast<RecordType>(this)) {
        unsigned components = recordTy->numComponents();
        for (unsigned i = 0; i < components; ++i) {
            if (recordTy->getComponentType(i)->involvesPercent())
                return true;
        }
        return false;
    }

    return false;
}

bool Type::isIndefiniteType() const
{
    if (const ArrayType *arrTy = dyn_cast<ArrayType>(this))
        return arrTy->isUnconstrained() || !arrTy->isStaticallyConstrained();
    return false;
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
// UniversalType

UniversalType *UniversalType::universal_integer = 0;
UniversalType *UniversalType::universal_access  = 0;
UniversalType *UniversalType::universal_fixed   = 0;
UniversalType *UniversalType::universal_real    = 0;

//===----------------------------------------------------------------------===//
// IncompleteType

IdentifierInfo *IncompleteType::getIdInfo() const
{
    if (IncompleteTypeDecl *decl = definingDecl.dyn_cast<IncompleteTypeDecl*>())
        return decl->getIdInfo();
    return definingDecl.get<IdentifierInfo*>();
}

IncompleteTypeDecl *IncompleteType::getDefiningDecl()
{
    IncompleteType *rootType = getRootType();
    return rootType->definingDecl.get<IncompleteTypeDecl*>();
}

bool IncompleteType::hasCompletion() const
{
    return getDefiningDecl()->hasCompletion();
}

PrimaryType *IncompleteType::getCompleteType()
{
    assert(hasCompletion() && "Type does not have a completion!");
    return getDefiningDecl()->getCompletion()->getType();
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

    if (const Range *constraint = getConstraint()) {
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

    if (const Range *constraint = target->getConstraint()) {
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
    UnconstrainedEnumType(EnumerationType *base,
                          EnumerationDecl *decl = 0)
        : EnumerationType(UnconstrainedEnumType_KIND, base),
          definingDecl(decl) {
        assert(decl == 0 || decl->isSubtypeDeclaration());
    }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return definingDecl == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this subtype is anonymous, the identifier returned is that of the
    /// root type.
    IdentifierInfo *getIdInfo() const {
        if (isAnonymous())
            return getAncestorType()->getIdInfo();
        else
            return definingDecl->getIdInfo();
    }

    //@{
    /// Returns the defining declaration of this subtype or null if this is an
    /// anonymous subtype.
    const EnumerationDecl *getDefiningDecl() const { return definingDecl; }
    EnumerationDecl *getDefiningDecl() { return definingDecl; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const UnconstrainedEnumType *node) { return true; }
    static bool classof(const EnumerationType *node) {
        return node->getEnumKind() == UnconstrainedEnumType_KIND;
    }

private:
    EnumerationDecl *definingDecl;
};

class ConstrainedEnumType : public EnumerationType {

public:
    ConstrainedEnumType(EnumerationType *base,
                        Expr *lowerBound, Expr *upperBound,
                        EnumerationDecl *decl = 0)
        : EnumerationType(ConstrainedEnumType_KIND, base),
          constraint(new Range(lowerBound, upperBound, base)),
          definingDecl(decl) { }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return definingDecl == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this is an anonymous type, returns the identifier of the underlying
    /// root type.
    IdentifierInfo *getIdInfo() const {
        if (isAnonymous())
            return getAncestorType()->getIdInfo();
        else
            return definingDecl->getIdInfo();
    }

    //@{
    /// Returns the constraint on this subtype.
    Range *getConstraint() { return constraint; }
    const Range *getConstraint() const { return constraint; }
    //@}

    /// \brief Returns true if the constraint bounds for this subtype are
    /// statically known.
    bool isStaticallyConstrained() const { return constraint->isStatic(); }

    //@{
    /// Returns the defining declaration of this subtype or null if this is an
    /// anonymous subtype.
    const EnumerationDecl *getDefiningDecl() const { return definingDecl; }
    EnumerationDecl *getDefiningDecl() { return definingDecl; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ConstrainedEnumType *node) { return true; }
    static bool classof(const EnumerationType *node) {
        return node->getEnumKind() == ConstrainedEnumType_KIND;
    }

private:
    Range *constraint;             ///< The constraint on this subtype.
    EnumerationDecl *definingDecl; ///< Defining decl or null.
};

} // end anonymous namespace.

RootEnumType::RootEnumType(AstResource &resource, EnumerationDecl *decl)
    : EnumerationType(RootEnumType_KIND, 0),
      definingDecl(decl)
{
    // Build the base unconstrained subtype node.
    baseType = cast<UnconstrainedEnumType>(resource.createEnumSubtype(this));
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

EnumerationDecl *EnumerationType::getDefiningDecl()
{
    RootEnumType *root = cast<RootEnumType>(getRootType());
    return root->getDefiningDecl();
}

bool EnumerationType::isCharacterType() const
{
    return getDefiningDecl()->isCharacterType();
};

uint64_t EnumerationType::getNumLiterals() const
{
    return cast<RootEnumType>(getRootType())->getNumLiterals();
}

Range *EnumerationType::getConstraint()
{
    if (ConstrainedEnumType *Ty = dyn_cast<ConstrainedEnumType>(this))
        return Ty->getConstraint();
    return 0;
}

const Range *EnumerationType::getConstraint() const
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


PosAD *EnumerationType::getPosAttribute()
{
    assert(false && "Enumeration attributes not yet implemented!");
    return 0;
}

ValAD *EnumerationType::getValAttribute()
{
    assert(false && "Enumeration attributes not yet implemented!");
    return 0;
}

EnumerationType *EnumerationType::create(AstResource &resource,
                                         EnumerationDecl *decl)
{
    return new RootEnumType(resource, decl);
}

EnumerationType *EnumerationType::createSubtype(EnumerationType *type,
                                                EnumerationDecl *decl)
{
    return new UnconstrainedEnumType(type, decl);
}

EnumerationType *
EnumerationType::createConstrainedSubtype(EnumerationType *type,
                                          Expr *lowerBound, Expr *upperBound,
                                          EnumerationDecl *decl)
{
    return new ConstrainedEnumType(type, lowerBound, upperBound, decl);
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
    ConstrainedIntegerType(IntegerType *base, Expr *lower, Expr *upper,
                           IntegerDecl *decl = 0)
        : IntegerType(ConstrainedIntegerType_KIND, base),
          constraint(new Range(lower, upper, base)),
          definingDecl(decl) { }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return definingDecl == 0; }

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this is an anonymous subtype, the identifier returned is that of the
    /// root type.
    IdentifierInfo *getIdInfo() const {
        if (isAnonymous())
            return getAncestorType()->getIdInfo();
        else
            return definingDecl->getIdInfo();
    }

    //@{
    /// Returns the constraint on this subtype.
    Range *getConstraint() { return constraint; }
    const Range *getConstraint() const { return constraint; }
    //@}

    /// \brief Returns true if the constraint bounds for this subtype are
    /// statically constrained.
    bool isStaticallyConstrained() const { return constraint->isStatic(); }

    //@{
    /// Returns the defining declaration of this subtype or null if this is an
    /// anonymous subtype.
    const IntegerDecl *getDefiningDecl() const { return definingDecl;  }
    IntegerDecl *getDefiningDecl() { return definingDecl; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ConstrainedIntegerType *node) { return true; }
    static bool classof(const IntegerType *node) {
        return node->getIntegerKind() == ConstrainedIntegerType_KIND;
    }

private:
    Range *constraint;          ///< subtype constraints.
    IntegerDecl *definingDecl;  ///< Defining decl or null.
};

class UnconstrainedIntegerType : public IntegerType {

public:
    UnconstrainedIntegerType(IntegerType *base,
                             IntegerDecl *decl = 0)
        : IntegerType(UnconstrainedIntegerType_KIND, base),
          definingDecl(decl) {
        assert(decl == 0 || decl->isSubtypeDeclaration());
    }

    /// Returns true if this is an anonymous subtype.
    bool isAnonymous() const { return definingDecl == 0; }

    //@{
    /// Returns the defining decl of this subtype or null if this is an
    /// anonymous subtype.
    const IntegerDecl *getDefiningDecl() const { return definingDecl; }
    IntegerDecl *getDefiningDecl() { return definingDecl; }
    //@}

    /// Returns the identifier which most appropriately names this subtype.
    ///
    /// If this is an anonymous subtype, the identifier returned is that of the
    /// root type.
    IdentifierInfo *getIdInfo() const {
        if (isAnonymous())
            return getAncestorType()->getIdInfo();
        else
            return definingDecl->getIdInfo();
    }

    // Support isa/dyn_cast.
    static bool classof(const UnconstrainedIntegerType *node) { return true; }
    static bool classof(const IntegerType *node) {
        return node->getIntegerKind() == UnconstrainedIntegerType_KIND;
    }

private:
    IntegerDecl *definingDecl;  ///< Defining decl or null.
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

    // Build the base unconstrained subtype node.
    baseType = cast<UnconstrainedIntegerType>
        (resource.createIntegerSubtype(this));
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
                                        IntegerDecl *decl)
{
    return new UnconstrainedIntegerType(type, decl);
}

IntegerType *IntegerType::createConstrainedSubtype(IntegerType *type,
                                                   Expr *lowerBound,
                                                   Expr *upperBound,
                                                   IntegerDecl *decl)
{
    return new ConstrainedIntegerType(type, lowerBound, upperBound, decl);
}

IntegerType *IntegerType::getBaseSubtype()
{
    RootIntegerType *root = cast<RootIntegerType>(this->getRootType());
    return root->getBaseSubtype();
}

Range *IntegerType::getConstraint()
{
    ConstrainedIntegerType *Ty;
    Ty = dyn_cast<ConstrainedIntegerType>(this);
    return Ty ? Ty->getConstraint() : 0;
}

const Range *IntegerType::getConstraint() const {
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

PosAD *IntegerType::getPosAttribute()
{
    return getDefiningDecl()->getPosAttribute();
}

ValAD *IntegerType::getValAttribute()
{
    return getDefiningDecl()->getValAttribute();
}

//===----------------------------------------------------------------------===//
// ArrayType

ArrayType::ArrayType(ArrayDecl *decl, unsigned rank, DiscreteType **indices,
                     Type *component, bool isConstrained)
    : CompositeType(AST_ArrayType, 0, false),
      indices(indices, indices + rank),
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
    : CompositeType(AST_ArrayType, rootType, true),
      indices(indices, indices + rootType->getRank()),
      componentType(rootType->getComponentType()),
      definingDecl(name)
{
    setConstraintBit();
    assert(this->isSubtype());
}

ArrayType::ArrayType(IdentifierInfo *name, ArrayType *rootType)
    : CompositeType(AST_ArrayType, rootType, true),
      indices(rootType->begin(), rootType->end()),
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
    return getAncestorType()->getIdInfo();
}

uint64_t ArrayType::length() const
{
    assert(isConstrained() &&
           "Cannot determine length of unconstrained arrays!");

    const DiscreteType *indexTy = getIndexType(0);
    const Range *constraint = indexTy->getConstraint();

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

bool ArrayType::isStaticallyConstrained() const
{
    if (!isConstrained())
        return false;
    for (const_iterator I = begin(); I != end(); ++I) {
        DiscreteType *Ty = *I;
        if (!Ty->isStaticallyConstrained())
            return false;
    }
    return true;
}

//===----------------------------------------------------------------------===//
// RecordType

RecordType::RecordType(RecordDecl *decl)
    : CompositeType(AST_RecordType, 0, false),
      definingDecl(decl) { }

RecordType::RecordType(RecordType *rootType, IdentifierInfo *name)
  : CompositeType(AST_RecordType, rootType, true),
    definingDecl(name) { }

RecordDecl *RecordType::getDefiningDecl()
{
    return getRootType()->definingDecl.get<RecordDecl*>();
}

IdentifierInfo *RecordType::getIdInfo() const
{
    if (IdentifierInfo *idInfo = definingDecl.dyn_cast<IdentifierInfo*>())
        return idInfo;
    return getAncestorType()->getIdInfo();
}

unsigned RecordType::numComponents() const
{
    return getDefiningDecl()->numComponents();
}

Type *RecordType::getComponentType(unsigned i)
{
    return getDefiningDecl()->getComponent(i)->getType();
}

//===----------------------------------------------------------------------===//
// AccessType

AccessType::AccessType(AccessDecl *decl, Type *targetType)
    : PrimaryType(AST_AccessType, 0, false),
      targetType(targetType),
      definingDecl(decl) { }

AccessType::AccessType(AccessType *rootType, IdentifierInfo *name)
    : PrimaryType(AST_AccessType, rootType, true),
      targetType(rootType->getTargetType()),
      definingDecl(name) { }

AccessDecl *AccessType::getDefiningDecl()
{
    return getRootType()->definingDecl.get<AccessDecl*>();
}

IdentifierInfo *AccessType::getIdInfo() const
{
    if (IdentifierInfo *idInfo = definingDecl.dyn_cast<IdentifierInfo*>())
        return idInfo;
    return getAncestorType()->getIdInfo();
}




