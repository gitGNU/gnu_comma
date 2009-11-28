//===-- ast/Type.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_TYPE_HDR_GUARD
#define COMMA_AST_TYPE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Constraint.h"
#include "comma/ast/Range.h"
#include "comma/basic/ParameterModes.h"

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/Support/Casting.h"

namespace comma {

//===----------------------------------------------------------------------===//
// Type

class Type : public Ast {

public:
    virtual ~Type() { }

    /// The following enumeration lists the "interesting" language-defined
    /// classes.
    enum Classification {
        CLASS_Scalar,
        CLASS_Discrete,
        CLASS_Enum,
        CLASS_Integer,
        CLASS_Composite,
        CLASS_Array,
        CLASS_String
    };

    /// Returns true if this type is a member of the given classification.
    bool memberOf(Classification ID) const;

    /// Returns true if this type denotes a scalar type.
    bool isScalarType() const;

    /// Returns true if this type denotes a discrete type.
    bool isDiscreteType() const;

    /// Returns true if this type denotes an integer type.
    bool isIntegerType() const;

    /// Returns true if this type denotes an enumeration type.
    bool isEnumType() const;

    /// Returns true if this type denotes a composite type.
    bool isCompositeType() const;

    /// Returns true if this type denotes an array type.
    bool isArrayType() const;

    /// Returns true if this type denotes a string type.
    bool isStringType() const;

    ArrayType *getAsArrayType();
    IntegerType *getAsIntegerType();
    EnumerationType *getAsEnumType();

    static bool classof(const Type *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesType();
    }

protected:
    Type(AstKind kind) : Ast(kind) {
        // Types are never directly deletable -- they are always owned by a
        // containing node.
        deletable = false;
        assert(this->denotesType());
    }

private:
    Type(const Type &);         // Do not implement.
};

//===----------------------------------------------------------------------===//
// SubroutineType
class SubroutineType : public Type {

public:
    virtual ~SubroutineType() { delete[] argumentTypes; }

    /// Returns the number of arguments accepted by this type.
    unsigned getArity() const { return numArguments; }

    /// Returns the type of the i'th parameter.
    Type *getArgType(unsigned i) const { return argumentTypes[i]; }

    /// Iterators over the argument types.
    typedef Type **arg_type_iterator;
    arg_type_iterator begin() const { return argumentTypes; }
    arg_type_iterator end() const {
        return argumentTypes + numArguments; }

    // Support isa and dyn_cast.
    static bool classof(const SubroutineType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubroutineType();
    }

protected:
    SubroutineType(AstKind kind, Type **argTypes, unsigned numArgs);

    Type **argumentTypes;
    unsigned numArguments;
};

//===----------------------------------------------------------------------===//
// FunctionType
class FunctionType : public SubroutineType, public llvm::FoldingSetNode {

public:
    /// Returns the result type of this function.
    Type *getReturnType() const { return returnType; }

    /// Profile implementation for use by llvm::FoldingSet.
    void Profile(llvm::FoldingSetNodeID &ID) {
        Profile(ID, argumentTypes, numArguments, returnType);
    }

    // Support isa and dyn_cast.
    static bool classof(const FunctionType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionType;
    }

private:
    Type *returnType;

    /// Function types are constructed thru an AstResource.
    friend class AstResource;

    FunctionType(Type **argTypes, unsigned numArgs, Type *returnType)
        : SubroutineType(AST_FunctionType, argTypes, numArgs),
          returnType(returnType) { }

    /// Profiler used by AstResource to unique function type nodes.
    static void Profile(llvm::FoldingSetNodeID &ID,
                        Type **argTypes, unsigned numArgs,
                        Type *returnType) {
        for (unsigned i = 0; i < numArgs; ++i)
            ID.AddPointer(argTypes[i]);
        ID.AddPointer(returnType);
    }
};

//===----------------------------------------------------------------------===//
// ProcedureType
class ProcedureType : public SubroutineType, public llvm::FoldingSetNode {

public:
    /// Profile implementation for use by llvm::FoldingSet.
    void Profile(llvm::FoldingSetNodeID &ID) {
        Profile(ID, argumentTypes, numArguments);
    }

    // Support isa and dyn_cast.
    static bool classof(const ProcedureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureType;
    }

private:
    /// ProcedureTypes are constructed thru AstResource.
    friend class AstResource;

    ProcedureType(Type **argTypes, unsigned numArgs)
        : SubroutineType(AST_ProcedureType, argTypes, numArgs) { }

    /// Profiler used by AstResource to unique procedure type nodes.
    static void Profile(llvm::FoldingSetNodeID &ID,
                        Type **argTypes, unsigned numArgs) {
        if (numArgs)
            for (unsigned i = 0; i < numArgs; ++i)
                ID.AddPointer(argTypes[i]);
        else
            ID.AddPointer(0);
    }
};

//===----------------------------------------------------------------------===//
// PrimaryType
//
/// The PrimaryType class forms the principle root of the type hierarchy.  Most
/// type nodes inherit from PrimaryType, with the notable exception of
/// SubroutineType.
class PrimaryType : public Type {

public:
    /// Returns true if this node denotes a subtype.
    bool isSubtype() const { return rootOrParentType.getInt(); }

    /// Returns true if this node denotes a root type.
    bool isRootType() const { return !isSubtype(); }

    //@{
    /// Returns the root type of this type.  If this is a root type, returns a
    /// pointer to this, otherwise the type of this subtype is returned.
    const PrimaryType *getRootType() const {
        return isRootType() ? this : rootOrParentType.getPointer();
    }
    PrimaryType *getRootType() {
        return isRootType() ? this : rootOrParentType.getPointer();
    }
    //@}

    /// Returns true if this is a derived type.
    bool isDerivedType() const {
        const PrimaryType *root = getRootType();
        return root->rootOrParentType.getPointer() != 0;
    }

    //@{
    /// \brief Returns the parent type of this type, or null if isDerivedType()
    /// returns false.
    PrimaryType *getParentType() {
        PrimaryType *root = getRootType();
        return root->rootOrParentType.getPointer();
    }
    const PrimaryType *getParentType() const {
        const PrimaryType *root = getRootType();
        return root->rootOrParentType.getPointer();
    }
    //@}

    /// Returns true if this type is constrained.
    ///
    /// \note Default implementation returns false.
    virtual bool isConstrained() const { return false; }

    /// \brief Returns true if this type is constrained, but specificly by an
    /// initial value.
    ///
    /// \note Types which are constrained by an initial value do not have an
    /// associated Constraint object.
    ///
    /// \note Default implementation returns false.
    virtual bool isConstrainedByInitialValue() const { return false; }

    //@{
    /// \brief Returns the Constraint object associated with this type, or null
    /// if there is no associated constraint.
    ///
    /// Only subtypes have constraints.  If this is a root type, then this
    /// method will always return null.  Also, if this type is constrained by an
    /// initial value, there a Constraint object is not available.
    ///
    /// \note Default implementation returns null.
    virtual Constraint *getConstraint() { return 0; }
    virtual const Constraint *getConstraint() const { return 0; }
    //@}

    /// Returns true if this type is a subtype of the given type.
    ///
    /// All types are considered to be subtypes of themselves.
    bool isSubtypeOf(const PrimaryType *type) const {
        return (type == this || getRootType() == type->getRootType());
    }

    // Support isa/dyn_cast.
    static bool classof(const PrimaryType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesPrimaryType();
    }

protected:
    /// Protected constructor for primary types.
    ///
    /// \param kind The concrete kind tag for this node.
    ///
    /// \param rootOrParent If this is to represent a root type, then this
    /// argument is a pointer to the parent type or null.  If this is to
    /// represent a subtype, then rootOrParent should point to the type of this
    /// subtype.
    ///
    /// \param subtype When true, the type under construction is a subtype.
    /// When false, the type is a root type.
    PrimaryType(AstKind kind, PrimaryType *rootOrParent, bool subtype)
        : Type(kind) {
        assert(this->denotesPrimaryType());
        if (subtype) {
            rootOrParentType.setPointer(rootOrParent->getRootType());
            rootOrParentType.setInt(true);
        }
        else {
            rootOrParentType.setPointer(rootOrParent);
            rootOrParentType.setInt(false);
        }
    }

private:
    /// The following field encapsulates a bit which marks this node as either a
    /// subtype or root type, and a pointer to the parent type or root type.
    ///
    /// When this type denotes a subtype, the following field contains a link to
    /// the root type (the type of the subtype).  Otherwise, this is a root type
    /// and rootOrParentType points to the parent type or null.
    llvm::PointerIntPair<PrimaryType*, 1, bool> rootOrParentType;
};


//===----------------------------------------------------------------------===//
// DomainType
class DomainType : public PrimaryType {

public:
    /// Returns the defining identifier of this type.
    IdentifierInfo *getIdInfo() const;

    /// Returns the defining identifier of this type as a C-string.
    const char *getString() const { return getIdInfo()->getString(); }

    /// Returns true if this node is a percent node.
    bool denotesPercent() const { return getPercentDecl() != 0; }

    /// Returns true if this type involves a percent node.
    ///
    /// More precisely, this method returns true if the node itself denotes
    /// percent, or (if this type corresponds to a parameterized instance) if
    /// any argument involves percent (applying this definition recursively).
    bool involvesPercent() const;

    /// Returns true if the underlying declaration is an DomainInstanceDecl.
    bool isConcrete() const { return getInstanceDecl() != 0; }

    /// Returns true if the underlying declaration is an AbstractDomainDecl.
    bool isAbstract() const { return getAbstractDecl() != 0; }

    //@
    /// Return the associated DomainTypeDecl.
    const DomainTypeDecl *getDomainTypeDecl() const;
    DomainTypeDecl *getDomainTypeDecl();
    //@}

    //@{
    /// If this node represents %, return the associated PercentDecl, else null.
    const PercentDecl *getPercentDecl() const;
    PercentDecl *getPercentDecl();
    //@}

    //@
    /// If this node is concrete, return the underlying DomainInstanceDecl, else
    /// null.
    const DomainInstanceDecl *getInstanceDecl() const;
    DomainInstanceDecl *getInstanceDecl();
    //@}

    //@{
    /// If this node is abstract, return underlying AbstractDomainDecl, else
    /// null.
    const AbstractDomainDecl *getAbstractDecl() const;
    AbstractDomainDecl *getAbstractDecl();
    //@}

    //@{
    /// Specialize PrimaryType::getRootType().
    DomainType *getRootType() {
        return llvm::cast<DomainType>(PrimaryType::getRootType());
    }
    const DomainType *getRootType() const {
        return llvm::cast<DomainType>(PrimaryType::getRootType());
    }
    //@}

    /// Support isa and dyn_cast.
    static bool classof(const DomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainType;
    }

private:
    /// Creates a type representing the given domain type declaration.
    DomainType(DomainTypeDecl *DTDecl);

    /// Creates a subtype of the given domain type.
    DomainType(DomainType *rootType, IdentifierInfo *name);

    /// Domain types are allocated and managed by AstResource.
    friend class AstResource;

    /// When root domain types are constructed, this union contains a pointer to
    /// the corresponding domain declaration.  For subtypes, this is a pointer
    /// to the identifier info naming the subtype.
    llvm::PointerUnion<DomainTypeDecl*, IdentifierInfo*> definingDecl;
};

//===----------------------------------------------------------------------===//
// DiscreteType
//
/// The DiscreteType class forms a common base for integer and enumeration
/// types.
class DiscreteType : public PrimaryType {

public:
    /// Returns the defining identifier for this type.
    virtual IdentifierInfo *getIdInfo() const = 0;

    /// Returns the upper limit for this type.
    ///
    /// The upper limit is the greatest value which can be represented by the
    /// underlying root type.  Note that this is not a bound as expressed via a
    /// subtype constraint.
    virtual void getUpperLimit(llvm::APInt &res) const = 0;

    /// Returns the lower limit for this type.
    ///
    /// The lower limit is the smallest value which can be represented by the
    /// underlying root type.  Note that this is not a bound as expressed via a
    /// subtype constraint.
    virtual void getLowerLimit(llvm::APInt &res) const = 0;

    /// Returns the number of bits needed to represent this type.
    ///
    /// The value returned by this method is equivalent to Size attribute.  The
    /// number returned specifies the minimum number of bits needed to represent
    /// values of this type, as opposed to the number of bits used to represent
    /// values of this type at runtime.
    virtual uint64_t getSize() const = 0;

    /// Returns true if this DiscreteType contains another.
    ///
    /// This type and the target type must be of the same category.  That is,
    /// both must be integer, enumeration, or (when implemented) modular types.
    ///
    /// Containment is with respect to the bounds on the types.  If a type is
    /// constrained, then the constraint is used for the bounds, otherwise the
    /// representational limits of the root type are used.
    ///
    /// If this type is constrained to a null range it can never contain the
    /// target, including other null types (with the only exception being that
    /// all types trivially contains themselves).  If this type is not
    /// constrained to a null range, then it always contains target type that
    /// is.
    ///
    /// If this type has a non-static constraint, this method always returns
    /// false.  If the target has a non-static constraint but the bounds for
    /// this type are known, containment is known only if this type contains the
    /// root type of the target.
    bool contains(const DiscreteType *target) const;

    /// Returns true if this denotes a signed discrete type.
    ///
    /// Currently, Integers are signed while enumerations are unsigned.
    bool isSigned() const;

    //@{
    /// Specialization of PrimaryType::getRootType().
    const DiscreteType *getRootType() const {
        return llvm::cast<DiscreteType>(PrimaryType::getRootType());
    }
    DiscreteType *getRootType() {
        return llvm::cast<DiscreteType>(PrimaryType::getRootType());
    }
    //@}

    //@{
    /// Specialization of PrimaryType::getConstraint().
    virtual RangeConstraint *getConstraint() = 0;
    virtual const RangeConstraint *getConstraint() const = 0;
    //@}

    /// Returns true if this type is constrained and the constraints are static.
    bool isStaticallyConstrained() const {
        if (const Range *range = getConstraint())
            return range->isStatic();
        return false;
    }

    // Support isa/dyn_cast.
    static bool classof(const DiscreteType *node) { return true; }
    static bool classof(const Ast *node) {
        return denotesDiscreteType(node->getKind());
    }

protected:
    DiscreteType(AstKind kind, DiscreteType *rootOrParent, bool subtype)
        : PrimaryType(kind, rootOrParent, subtype) {
        assert(denotesDiscreteType(kind));
    }

    // Convinience utility for subclasses.  Returns the number of bits that
    // should be used for the size of the type, given the minimal number of bits
    // needed to represent the entity.
    static unsigned getPreferredSize(uint64_t bits);

private:
    static bool denotesDiscreteType(AstKind kind) {
        return (kind == AST_EnumerationType || kind == AST_IntegerType);
    }
};

//===----------------------------------------------------------------------===//
// EnumerationType
class EnumerationType : public DiscreteType {

public:
    virtual ~EnumerationType() { }

    /// Returns the lower limit for this type.
    ///
    /// \see DiscreteType::getLowerLimit().
    void getLowerLimit(llvm::APInt &res) const;

    /// Returns the upper limit for this type.
    ///
    /// \see DiscreteType::getUpperLimit().
    void getUpperLimit(llvm::APInt &res) const;

    /// Returns the number of bits needed to represent this type.
    ///
    /// \see DiscreteType::getSize().
    uint64_t getSize() const;

    /// Returns the number of literals in this enumeration type.
    uint64_t getNumLiterals() const;

    /// Returns true if this enumeration type is a character type.
    bool isCharacterType() const;

    /// Returns true if this type is constrained.
    bool isConstrained() const { return getConstraint() != 0; }

    //@{
    /// \brief Returns the RangeConstraint associated with this EnumerationType,
    /// or null if this is an unconstrained type.
    RangeConstraint *getConstraint();
    const RangeConstraint *getConstraint() const;
    //@}

    //@{
    /// Specialize PrimaryType::getRootType().
    EnumerationType *getRootType() {
        return llvm::cast<EnumerationType>(PrimaryType::getRootType());
    }
    const EnumerationType *getRootType() const {
        return llvm::cast<EnumerationType>(PrimaryType::getRootType());
    }
    //@}

    //@{
    /// Returns the base (unconstrained) subtype of this enumeration type.
    EnumerationType *getBaseSubtype();
    const EnumerationType *getBaseSubtype() const;
    //@}

    // Support isa and dyn_cast.
    static bool classof(const EnumerationType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumerationType;
    }

private:
    /// \name Static constructors.
    ///
    /// The following factory functions are called by AstResource to build
    /// various kinds if EnumerationType nodes.

    /// Builds a root enumeration type.
    static EnumerationType *create(AstResource &resource,
                                   EnumerationDecl *decl);

    /// Builds an unconstrained enumeration subtype.
    static EnumerationType *
    createSubtype(EnumerationType *rootType, IdentifierInfo *name);

    /// Builds a constrained enumeration subtype.
    static EnumerationType *
    createConstrainedSubtype(EnumerationType *rootType,
                             Expr *lowerBound, Expr *upperBound,
                             IdentifierInfo *name);
    //@}
    friend class AstResource;

protected:
    /// EnumerationType nodes are implemented using three internal classes
    // representing the root, constrained, and unconstrained cases.  The
    // following enumeration identifiers each of these classes and is encoded
    // into the AST::bits field.
    enum EnumKind {
        RootEnumType_KIND,
        UnconstrainedEnumType_KIND,
        ConstrainedEnumType_KIND
    };

    /// Returns true if the given kind denotes a subtype.
    static bool isSubtypeKind(EnumKind kind) {
        return (kind == UnconstrainedEnumType_KIND ||
                kind == ConstrainedEnumType_KIND);
    }

    /// Constructor for the internal subclasses (not for use by AstResource).
    EnumerationType(EnumKind kind, EnumerationType *rootOrParent)
        : DiscreteType(AST_EnumerationType, rootOrParent, isSubtypeKind(kind)) {
        bits = kind;
    }

    /// Returns the underlying enumeration declaration for this type.
    const EnumerationDecl *getDeclaration() const;

public:
    /// Returns the EnumKind of this node.  For internal use only.
    EnumKind getEnumKind() const { return EnumKind(bits); }
};

//===----------------------------------------------------------------------===//
// IntegerType
//
// These nodes represent ranged, signed, integer types.  They are allocated and
// owned by an AstResource instance.
class IntegerType : public DiscreteType {

public:
    virtual ~IntegerType() { }

    /// Returns the lower limit for this type.
    ///
    /// \see DiscreteType::getLowerLimit().
    void getLowerLimit(llvm::APInt &res) const;

    /// Returns the upper limit for this type.
    ///
    /// \see DiscreteType::getUpperLimit().
    void getUpperLimit(llvm::APInt &res) const;

    /// Returns true if the base integer type can represent the given value
    /// (interpreted as signed).
    bool baseContains(const llvm::APInt &value) const;

    /// Returns the number of bits needed to represent this type.
    ///
    /// \see DiscreteType::getSize();
    uint64_t getSize() const;

    /// Returns the base subtype.
    ///
    /// The base subtype is a distinguished unconstrained subtype corresponding
    /// to the attribute S'Base.
    IntegerType *getBaseSubtype();

    /// Returns true if this type is constrained.
    bool isConstrained() const { return getConstraint() != 0; }

    //@{
    /// \brief Returns the RangeConstraint associated with this IntegerType, or
    /// null if this is an unconstrained type.
    RangeConstraint *getConstraint();
    const RangeConstraint *getConstraint() const;
    //@}

    //@{
    /// Specialize PrimaryType::getRootType().
    IntegerType *getRootType() {
        return llvm::cast<IntegerType>(PrimaryType::getRootType());
    }
    const IntegerType *getRootType() const {
        return llvm::cast<IntegerType>(PrimaryType::getRootType());
    }
    //@}

    /// Support isa and dyn_cast.
    static bool classof(const IntegerType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerType;
    }

private:
    /// \name Static constructors.
    ///
    /// The following factory functions are called by AstResource to build
    /// various types of IntegerType nodes.
    //@{

    /// Builds a root integer type with the given static bounds.
    static IntegerType *create(AstResource &resource, IntegerDecl *decl,
                               const llvm::APInt &lower,
                               const llvm::APInt &upper);

    /// Builds an unconstrained integer subtype.
    static IntegerType *
    createSubtype(IntegerType *rootType, IdentifierInfo *name);

    /// Builds a constrained integer subtype.
    static IntegerType *
    createConstrainedSubtype(IntegerType *rootType,
                             Expr *lowerBound, Expr *upperBound,
                             IdentifierInfo *name);
    //@}
    friend class AstResource;

protected:
    /// IntegerType nodes are implemented using three internal classes
    /// represeting the root, constrained, and unconstrained cases.  The
    /// following enumeration identifies each of these classes and is encoded
    /// into the AST::bits field.
    enum IntegerKind {
        RootIntegerType_KIND,
        UnconstrainedIntegerType_KIND,
        ConstrainedIntegerType_KIND
    };

    /// Returns true if the given kind denotes a subtype.
    static bool isSubtypeKind(IntegerKind kind) {
        return (kind == UnconstrainedIntegerType_KIND ||
                kind == ConstrainedIntegerType_KIND);
    }

    /// Constructor for the internal subclasses (not for use by AstResource).
    IntegerType(IntegerKind kind, IntegerType *rootOrParent)
        : DiscreteType(AST_IntegerType, rootOrParent, isSubtypeKind(kind)) {
        bits = kind;
    }

public:
    /// Returns the IntegerKind of this node.  For internal use only.
    IntegerKind getIntegerKind() const { return IntegerKind(bits); }
};

//===----------------------------------------------------------------------===//
// ArrayType
//
// These nodes describe the index profile and component type of an array type.
// They are allocated and owned by an AstResource instance.
class ArrayType : public PrimaryType {

public:
    /// Returns the identifier associated with this array type.
    IdentifierInfo *getIdInfo() const;

    /// Returns the rank (dimensionality) of this array type.
    unsigned getRank() const {
        return constraint.numConstraints();
    }

    /// Returns true if this is a vector type (an array of rank 1).
    bool isVector() const { return getRank() == 1; }

    /// Return the length of the first dimension.  This operation is valid only
    /// if this is a statically constrained array type.
    uint64_t length() const;

    //@{
    /// Returns the i'th index type of this array.
    const DiscreteType *getIndexType(unsigned i) const {
        return constraint.getConstraint(i);
    }
    DiscreteType *getIndexType(unsigned i) {
        return constraint.getConstraint(i);
    }
    //@}

    /// \name Index Type Iterators.
    ///
    /// Iterators over the index types of this array.
    //@{
    typedef IndexConstraint::iterator index_iterator;
    index_iterator begin_indices() const { return constraint.begin(); }
    index_iterator end_indices() const { return constraint.end(); }
    //@}

    /// Returns the component type of this array.
    Type *getComponentType() const { return componentType; }

    /// Returns true if this type is constrained.
    bool isConstrained() const { return constraintBit(); }

    /// Returns true if this type is constrained by an initial value.
    bool isConstrainedByInitialValue() const { return constrainedByInitBit(); }

    //@{
    /// Specialization of PrimaryType::getConstraint();
    IndexConstraint *getConstraint() {
        if (isConstrained())
            return &constraint;
        return 0;
    }

    const IndexConstraint *getConstraint() const {
        if (isConstrained())
            return &constraint;
        return 0;
    }
    //@}

    /// Returns true if this array type is statically constrained.
    bool isStaticallyConstrained() const {
        if (!isConstrained())
            return false;
        for (index_iterator I = begin_indices(); I != end_indices(); ++I) {
            DiscreteType *Ty = *I;
            if (!Ty->isStaticallyConstrained())
                return false;
        }
        return true;
    }

    //@{
    /// Specialize PrimaryType::getRootType().
    ArrayType *getRootType() {
        return llvm::cast<ArrayType>(PrimaryType::getRootType());
    }
    const ArrayType *getRootType() const {
        return llvm::cast<ArrayType>(PrimaryType::getRootType());
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const ArrayType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ArrayType;
    }

private:
    /// Creates a root array type.
    ArrayType(ArrayDecl *decl, unsigned rank, DiscreteType **indices,
              Type *component, bool isConstrained);

    /// Creates a constrained array subtype.
    ArrayType(IdentifierInfo *name, ArrayType *rootType,
              DiscreteType **indices);

    /// Creates an unconstrained array subtype.
    ArrayType(IdentifierInfo *name, ArrayType *rootType);

    friend class AstResource;

    /// The following enumeration defines propertys of an array type which are
    /// encoded into the bits field of the node.
    enum PropertyTags {
        /// Set if the type is constrained.
        Constrained_PROP = 1,

        /// Set if the type is constrained by its initial value (Constraint_PROP
        /// is always set if this flag is true).
        Constrained_By_Init_PROP = 2
    };

    /// Returns true if this is a constrained array.
    bool constraintBit() const { return bits & Constrained_PROP; }

    /// Marks this as a constrained array type.
    void setConstraintBit() { bits |= Constrained_PROP; }

    /// Returns true if this type is constrained by an initial value.
    bool constrainedByInitBit() const {
        return bits & Constrained_By_Init_PROP;
    }

    /// Marks this as being constrained by an initial value (sets
    /// Constrained_PROP as well).
    void setConstrainedByInitBit() {
        setConstraintBit();
        bits |= Constrained_By_Init_PROP;
    }

    /// This class `abuses' the IndexConstraint class, using it to represent the
    /// index types for both constrained and unconstrained arrays.
    IndexConstraint constraint;

    /// The component type of this array.
    Type *componentType;

    /// The declaration node or, in the case of an array subtype, the defining
    /// identifier.
    ///
    /// \note This union will contain a subtype declaration instead of an
    /// identifier info once such nodes are supported.
    llvm::PointerUnion<ArrayDecl*, IdentifierInfo*> definingDecl;
};

//===----------------------------------------------------------------------===//
// CarrierType
//
// The type of carrier declarations.
class CarrierType : public PrimaryType {

public:
    /// Returns the defining identifier of this CarrierType.
    IdentifierInfo *getIdInfo() const;

    //@{
    /// Specializations of PrimaryType::getConstraint().
    ///
    /// \note CarrierType's are never constrained.
    Constraint *getConstraint() { return 0; }
    const Constraint *getConstraint() const { return 0; }
    //@}

    //@{
    /// Returns the declaration corresponding to this carrier.
    CarrierDecl *getDeclaration() { return definingDecl; }
    const CarrierDecl *getDeclaration() const { return definingDecl; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const CarrierType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_CarrierType;
    }

private:
    CarrierType(CarrierDecl *carrier, PrimaryType *type);
    friend class AstResource;

    CarrierDecl *definingDecl;
};

} // End comma namespace

#endif
