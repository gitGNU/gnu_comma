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
    IntegerSubType *getAsIntegerSubType();

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
// DomainType
class DomainType : public Type {

private:
    /// Domain types are created and owned by a unique DomainTypeDecl.
    friend class DomainTypeDecl;

    /// Creates a type representing the given domain type declaration.
    DomainType(DomainTypeDecl *DTDecl);

public:
    /// Returns the defining identifier of this type.
    IdentifierInfo *getIdInfo() const;

    /// Returns the defining identifier of this type as a C-string.
    const char *getString() const { return getIdInfo()->getString(); }

    /// Return the associated DomainTypeDecl.
    DomainTypeDecl *getDomainTypeDecl() const;

    /// Returns true if this node is a percent node.
    bool denotesPercent() const { return getPercentDecl() != 0; }

    /// Returns true if this type involves a percent node.
    ///
    /// More precisely, this method returns true if the node itself denotes
    /// percent, or (if this type corresponds to a parameterized instance) if
    /// any argument involves percent (applying this definition recursively).
    bool involvesPercent() const;

    /// If this node represents %, return the associated PercentDecl, else null.
    PercentDecl *getPercentDecl() const;

    /// Returns true if the underlying declaration is an DomainInstanceDecl.
    bool isConcrete() const { return getInstanceDecl() != 0; }

    /// If this node is concrete, return the underlying DomainInstanceDecl, else
    /// null.
    DomainInstanceDecl *getInstanceDecl() const;

    /// Returns true if the underlying declaration is an AbstractDomainDecl.
    bool isAbstract() const { return getAbstractDecl() != 0; }

    /// If this node is abstract, return underlying AbstractDomainDecl, else
    /// null.
    AbstractDomainDecl *getAbstractDecl() const;

    /// Support isa and dyn_cast.
    static bool classof(const DomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainType;
    }

private:
    Decl *declaration;
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
    SubroutineType(AstKind kind,
                   Type **argTypes, unsigned numArgs)
        : Type(kind),
          argumentTypes(0),
          numArguments(numArgs) {
        assert(this->denotesSubroutineType());
        if (numArgs > 0) {
            argumentTypes = new Type*[numArgs];
            std::copy(argTypes, argTypes + numArgs, argumentTypes);
        }
    }

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

    FunctionType(Type **argTypes, unsigned numArgs,
                 Type *returnType)
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
// EnumerationType
class EnumerationType : public Type {

public:
    /// Returns the number of distinct elements in this enumeration type.
    ///
    /// FIXME: This property should be removed in favour of more generic
    /// attributes such as First, Last, Size, etc.
    unsigned getNumElements() const { return numElems; }

    /// Marks this enumeration as a character type.
    void markAsCharacterType() { bits = 1; }

    /// Returns true if this enumeration type is a character type.
    bool isCharacterType() const { return bits == 1; }

    // Support isa and dyn_cast.
    static bool classof(const EnumerationType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumerationType;
    }

private:
    // Private constructor for use by AstResource to allocate EnumerationType
    // nodes.
    EnumerationType(unsigned numElems)
        : Type(AST_EnumerationType), numElems(numElems) { }

    friend class AstResource;

    unsigned numElems;          ///< Number of elements in this enum types.
};

//===----------------------------------------------------------------------===//
// IntegerType
//
// These nodes represent ranged, signed, integer types.  They are allocated and
// owned by an AstResource instance.
class IntegerType : public Type {

public:
    const llvm::APInt &getLowerBound() const { return low; }
    const llvm::APInt &getUpperBound() const { return high; }

    /// Returns the base subtype.
    IntegerSubType *getBaseSubType() const { return baseSubType; }

    /// Returns the number of bits needed to represent this integer type.
    unsigned getBitWidth() const { return low.getBitWidth(); }

    /// Returns true if this IntegerType contains another.
    ///
    /// Here, `contains' means that this integer type is wide enough to hold all
    /// values of the given type.
    bool contains(IntegerType *type) const {
        return getBitWidth() >= type->getBitWidth();
    }

    /// Returns true if this IntegerType contains the given Integer subtype.
    ///
    /// Again, `contains' means that this integer type is wide enough to hold
    /// all values of the given subtype.  Note that the given subtype may be of
    /// a base which is larger than this type -- in such a case the subtype must
    /// be constrained to a range within the representation of this integer type.
    bool contains(IntegerSubType *subtype) const;

    /// Support isa and dyn_cast;
    static bool classof(const IntegerType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerType;
    }

private:
    // Private constructor used by AstResource to allocate ranged integer types.
    IntegerType(AstResource &resource, IntegerDecl *decl,
                const llvm::APInt &low, const llvm::APInt &high);

    friend class AstResource;

    // Returns the minimun bit width needed to represent the given range.
    static unsigned getWidthForRange(const llvm::APInt &low,
                                     const llvm::APInt &high);

    void initBounds(const llvm::APInt &low, const llvm::APInt &high);

    // The lower and upper bounds for this type.
    llvm::APInt low;
    llvm::APInt high;

    // The base subtype.
    IntegerSubType *baseSubType;
};

//===----------------------------------------------------------------------===//
// ArrayType
//
// These nodes describe the index profile and component type of an array type.
// They are allocated and owned by an AstResource instance.
class ArrayType : public Type {

public:
    /// Returns the rank (dimensionality) of this array type.
    unsigned getRank() const { return rank; }

    /// Returns true if this is a vector type (an array of rank 1).
    bool isVector() const { return rank == 1; }

    /// Returns the i'th index type of this array.
    SubType *getIndexType(unsigned i) const {
        assert(i < rank && "Index is out of bounds!");
        return indexTypes[i];
    }

    /// Return the length of the first dimension.  This operation is valid only
    /// if this is a statically constrained array type.
    uint64_t length() const;

    /// \name Index Type Iterators.
    ///
    /// Iterators over the index types of this array.
    ///@{
    typedef SubType** index_iterator;
    index_iterator begin_indices() const { return &indexTypes[0]; }
    index_iterator end_indices() const { return &indexTypes[rank]; }
    ///@}

    /// Returns the component type of this array.
    Type *getComponentType() const { return componentType; }

    /// Returns true if this is a constrained array type.
    bool isConstrained() const { return bits & CONSTRAINT_BIT; }

    // Support isa and dyn_cast.
    static bool classof(const ArrayType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ArrayType;
    }

private:
    /// Constants for accessing the ast::bits field.
    enum {
        CONSTRAINT_BIT = (1 << 0)
    };

    // Private constructor used by AstResource to allocate array types.
    ArrayType(unsigned rank, SubType **indices, Type *component,
              bool isConstrained);

    friend class AstResource;

    unsigned rank;              ///< The dimensionality of this array.
    SubType **indexTypes;       ///< The index types of this array.
    Type *componentType;        ///< The component type of this array.
};

//===----------------------------------------------------------------------===//
// SubType
class SubType : public Type {

public:
    virtual ~SubType() { };

    /// Returns the parent type of this subtype.
    Type *getParentType() const { return ParentType; }

    /// Returns the type of this subtype.
    Type *getTypeOf() const;

    /// Returns true if this subtype is constrained.
    virtual bool isConstrained() const = 0;

    /// Returns the defining identifier of this subtype if it is named, else null.
    IdentifierInfo *getIdInfo() const { return DefiningIdentifier; }

    /// Returns a C-string representation of the defining identifier if this
    /// subtype is names, else null.
    const char *getString() const {
        if (IdentifierInfo *idInfo = getIdInfo())
            return idInfo->getString();
        return 0;
    }

    static bool classof(const SubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubType();
    }

protected:
    /// Constructs a named subtype.
    SubType(AstKind kind, IdentifierInfo *identifier, Type *type);

    /// Constructs an anonymous subtype.
    SubType(AstKind kind, Type *type);

    IdentifierInfo *DefiningIdentifier;
    Type *ParentType;
};


//===----------------------------------------------------------------------===//
// CarrierType
//
// The type of carrier declarations.
class CarrierType : public SubType {

public:
    CarrierType(CarrierDecl *carrier, Type *type);

    bool isConstrained() const { return false; }

    IdentifierInfo *getIdInfo() const;

    CarrierDecl *getDeclaration() { return declaration; }
    const CarrierDecl *getDeclaration() const { return declaration; }

    static bool classof(const CarrierType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_CarrierType;
    }

private:
    CarrierDecl *declaration;
};

//===----------------------------------------------------------------------===//
// ArraySubType
class ArraySubType : public SubType {

public:
    /// Defines a subtype of the given array type.  The constraint may be null
    /// to define an unconstrained subtype.
    ArraySubType(IdentifierInfo *identifier, ArrayType *type,
                 IndexConstraint *constraint)
        : SubType(AST_ArraySubType, identifier, type),
          constraint(constraint) { }

    /// Returns the ArrayType underlying this subtype.
    ArrayType *getTypeOf() const {
        return llvm::cast<ArrayType>(SubType::getTypeOf());
    }

    /// Returns the rank (dimensionality) of this array subtype.
    unsigned getRank() const { return getTypeOf()->getRank(); }

    /// Returns the component type of this array subtype.
    Type *getComponentType() const {
        return getTypeOf()->getComponentType();
    }

    /// Returns the i'th index type of this array subtype.
    SubType *getIndexType(unsigned i) const {
        if (isConstrained())
            return getIndexConstraint(i);
        return getTypeOf()->getIndexType(i);
    }

    /// Returns the length of the first dimension of this array.  This operation
    /// is valid iff the index type is statically constrained.
    uint64_t length() const;

    /// Returns true if this is a constrained subtype.
    bool isConstrained() const { return constraint != 0; }

    /// Returns the index constraint associated with this subtype, or null if
    /// there are not any.
    IndexConstraint *getConstraint() const { return constraint; }

    /// Returns the constraint for the i'th index.
    SubType *getIndexConstraint(unsigned i) const {
        return getConstraint()->getConstraint(i);
    }

    // FIXME: These iterators need to take our index constraints (if any) into
    // account.
    typedef ArrayType::index_iterator index_iterator;
    index_iterator begin_indices() const {
        return getTypeOf()->begin_indices();
    }
    index_iterator end_indices() const {
        return getTypeOf()->end_indices();
    }

    static bool classof(const ArraySubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ArraySubType;
    }

private:
    IndexConstraint *constraint;
};

//===----------------------------------------------------------------------===//
// EnumSubType
class EnumSubType : public SubType {

public:
    /// Returns true if this subtype is constrained.
    bool isConstrained() const { return false; }

    /// Returns the type of this enumeration subtype.
    EnumerationType *getTypeOf() const {
        return llvm::cast<EnumerationType>(SubType::getTypeOf());
    }

    static bool classof(const EnumSubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumSubType;
    }

private:
    // Defines an unconstrained subtype of the given enumeration type.
    EnumSubType(IdentifierInfo *identifier, EnumerationType *type)
        : SubType(AST_EnumSubType, identifier, type) { }

    friend class AstResource;
};

//===----------------------------------------------------------------------===//
// IntegerSubType
class IntegerSubType : public SubType {

public:
    /// Returns the type of this integer subtype.
    IntegerType *getTypeOf() const {
        return llvm::cast<IntegerType>(SubType::getTypeOf());
    }

    /// Returns true if this subtype is constrained.
    bool isConstrained() const { return constraint != 0; }

    /// Returns the range constraint of this subtype, or null if this subtype is
    /// not constrained.
    Range *getConstraint() const { return constraint; }

    /// Returns true if this type has a null range constraint.
    bool isNull() const { return isConstrained() && getConstraint()->isNull(); }

    /// Returns true if this subtype is staticly constrained.
    bool isStaticallyConstrained() const {
        return isConstrained() && getConstraint()->isStatic();
    }

    /// Returns true if this subtype contains another.
    ///
    /// One integer subtype contains another if the range of the former includes
    /// the range of the latter.
    bool contains(IntegerSubType *subtype) const;

    /// Returns true if this subtype contains the given integer type.
    ///
    /// An integer subtype contains another integer type if the range of the
    /// former spans the representational limits of the latter.
    bool contains(IntegerType *type) const;

    /// If this is a constrained subtype, the lower bound must be staticly
    /// constrained.  If constrained, returns the lower bound.  Otherwise,
    /// returns the lower bound of the base type.
    const llvm::APInt &getLowerBound() const {
        if (Range *range = getConstraint()) {
            assert(range->hasStaticLowerBound());
            return range->getStaticLowerBound();
        }
        else
            return getTypeOf()->getLowerBound();
    }

    /// If this is a constrained subtype, the upper bound must be staticly
    /// constrained.  If constrained, returns the upper bound.  Otherwise,
    /// returns the upper bound of the base type.
    const llvm::APInt &getUpperBound() const {
        if (Range *range = getConstraint()) {
            assert(range->hasStaticUpperBound());
            return range->getStaticUpperBound();
        }
        else
            return getTypeOf()->getUpperBound();
    }

    // Support isa/dyn_cast.
    static bool classof(const IntegerSubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerSubType;
    }

private:
    /// Constructs a named integer subtype of the given type and range
    /// constraints.
    IntegerSubType(IdentifierInfo *name, IntegerType *type,
                   Expr *low, Expr *high)
        : SubType(AST_IntegerSubType, name, type),
          constraint(Range::create(low, high)) { }

    /// Constructs a named, unconstrained integer subtype.
    IntegerSubType(IdentifierInfo *name, IntegerType *type)
        : SubType(AST_IntegerSubType, name, type),
          constraint(0) { }

    /// Allow AstResource to construct IntegerSubType nodes.
    friend class AstResource;

    Range *constraint;
};

} // End comma namespace

#endif
