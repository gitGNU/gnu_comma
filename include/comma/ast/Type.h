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
#include "comma/ast/AstRewriter.h"
#include "comma/ast/Constraint.h"
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

    /// Returns true if this is an anonymous type.
    ///
    /// By anonymous we mean something different here than how the spec reads.
    /// Technically, the only named types are SubType's.  But practically
    /// speaking we can almost always associate a name with a type, or rather, a
    /// declaration with a type.  This function returns true if there is a
    /// declaration associated with this type, and therefore a logical name is
    /// available identifying it.
    ///
    /// Some types are always anonymous.  Procedure types, for example, are
    /// uniqued and are never associated with a single declaration.
    bool isAnonymous() const { return getIdInfo() != 0; }

    /// Returns the defining identifier associated with this type, or null if
    /// this is an anonymous type.
    virtual IdentifierInfo *getIdInfo() const { return 0; }

    /// Returns true if this type denotes a scalar type.
    bool isScalarType() const;

    /// Returns true if this type denotes a discrete type.
    bool isDiscreteType() const;

    /// Returns true if this type denotes an integer type.
    bool isIntegerType() const;

    /// Returns true if this type denotes an enumeration type.
    bool isEnumType() const;

    /// Returns true if this type denotes an array type.
    bool isArrayType() const;

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
    EnumerationType(EnumerationDecl *decl);

    IdentifierInfo *getIdInfo() const;

    EnumerationDecl *getEnumerationDecl() { return declaration; }
    const EnumerationDecl *getEnumerationDecl() const { return declaration; }

    /// Returns the first subtype of this enumeration type.
    EnumSubType *getFirstSubType() const { return FirstSubType; }

    // Support isa and dyn_cast.
    static bool classof(const EnumerationType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumerationType;
    }

private:
    EnumerationDecl *declaration;
    EnumSubType *FirstSubType;
};

//===----------------------------------------------------------------------===//
// IntegerType
//
// These nodes represent ranged, signed, integer types.  They are allocated and
// owned by an AstResource instance.
class IntegerType : public Type {

public:
    IdentifierInfo *getIdInfo() const;

    const llvm::APInt &getLowerBound() const { return low; }
    const llvm::APInt &getUpperBound() const { return high; }

    /// Returns the number of bits needed to represent this integer type.
    unsigned getBitWidth() const { return low.getBitWidth(); }

    /// Returns the first subtype of this integer type.
    IntegerSubType *getFirstSubType() const { return FirstSubType; }

    /// Support isa and dyn_cast;
    static bool classof(const IntegerType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerType;
    }

private:
    // Private constructor used by AstResource to allocate ranged integer types.
    IntegerType(IntegerDecl *decl,
                const llvm::APInt &low, const llvm::APInt &high);

    friend class AstResource;

    // Returns the minimun bit width needed to represent the given range.
    static unsigned getWidthForRange(const llvm::APInt &low,
                                     const llvm::APInt &high);

    // Returns the base range used to represent the given range of values.
    static std::pair<llvm::APInt, llvm::APInt>
    getBaseRange(const llvm::APInt &low, const llvm::APInt &high);

    // The lower and upper bounds for this type.
    llvm::APInt low;
    llvm::APInt high;

    // First subtype.
    IntegerSubType *FirstSubType;

    // Base subtype.
    IntegerSubType *BaseSubType;

    // Declaration associated with this integer type.
    IntegerDecl *declaration;
};

//===----------------------------------------------------------------------===//
// ArrayType
//
// These nodes describe the index profile and component type of an array type.
// They are allocated and owned by an AstResource instance.
class ArrayType : public Type {

public:
    /// Returns the defining identifier of this array type.
    IdentifierInfo *getIdInfo() const;

    /// Returns the rank (dimensionality) of this array type.
    unsigned getRank() const { return rank; }

    /// Returns the i'th index type of this array.
    SubType *getIndexType(unsigned i) const {
        assert(i < rank && "Index is out of bounds!");
        return indexTypes[i];
    }

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

    /// Returns the first subtype of this array type.
    ArraySubType *getFirstSubType() const { return FirstSubType; }

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
    ArrayType(ArrayDecl *decl,
              unsigned rank, SubType **indices, Type *component,
              bool isConstrained);

    friend class AstResource;

    unsigned rank;              ///< The dimensionality of this array.
    SubType **indexTypes;       ///< The index types of this array.
    Type *componentType;        ///< The component type of this array.
    ArraySubType *FirstSubType; ///< First subtype of this array.
    ArrayDecl *declaration;     ///< Corresponding declaration.
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
    bool isConstrained() const { return SubTypeConstraint != 0; }

    /// Returns the defining identifier of this subtype if it is named,
    /// else the defining identifier of its first progenitor type.
    IdentifierInfo *getIdInfo() const {
        if (DefiningIdentifier)
            return DefiningIdentifier;
        return ParentType->getIdInfo();
    }

    /// Returns the constraint of this subtype if constrained, else null for
    /// unconstrained subtypes.
    Constraint *getConstraint() const { return SubTypeConstraint; }

    static bool classof(const SubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubType();
    }

protected:
    /// Constructs a named subtype.  The constraint may be null to define an
    /// unsconstrained subtype.
    SubType(AstKind kind, IdentifierInfo *identifier, Type *type,
            Constraint *constraint);

    /// Constructs an anonymous subtype.  The constraint may be null to define
    /// an unconstrained subtype.
    SubType(AstKind kind, Type *type, Constraint *constraint);

    IdentifierInfo *DefiningIdentifier;
    Type *ParentType;
    Constraint *SubTypeConstraint;
};


//===----------------------------------------------------------------------===//
// CarrierType
//
// The type of carrier declarations.  In the future this node could be combined
// into a general "type alias" node or similar.
class CarrierType : public SubType {

public:
    CarrierType(CarrierDecl *carrier, Type *type);

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
        : SubType(AST_ArraySubType, identifier, type, constraint) { }

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

    /// Returns the index constraint associated with this subtype, or null if
    /// there are not any.
    IndexConstraint *getConstraint() const {
        return llvm::cast<IndexConstraint>(SubTypeConstraint);
    }

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
};

//===----------------------------------------------------------------------===//
// EnumSubType
class EnumSubType : public SubType {

public:
    // Defines a subtype of the given enumeration type.  The constraint may be
    // null to define an unconstrained subtype.
    EnumSubType(IdentifierInfo *identifier, EnumerationType *type,
                RangeConstraint *constraint)
        : SubType(AST_EnumSubType, identifier, type, constraint) { }

    /// Returns the type of this enumeration subtype.
    EnumerationType *getTypeOf() const {
        return llvm::cast<EnumerationType>(SubType::getTypeOf());
    }

    static bool classof(const EnumSubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumSubType;
    }
};

//===----------------------------------------------------------------------===//
// IntegerSubType
class IntegerSubType : public SubType {

public:
    /// Constructs a named integer subtype of the given type and constraint.
    /// The constraint may be null to define an unconstrained subtype.
    IntegerSubType(IdentifierInfo *name, IntegerType *type,
                   RangeConstraint *constraint)
        : SubType(AST_IntegerSubType, name, type, constraint) { }

    /// Constructs an anonymous subtype of the given type and constraint.  The
    /// constraint may be null to define an unconstrained subtype.
    IntegerSubType(IntegerType *type, RangeConstraint *constraint)
        : SubType(AST_IntegerSubType, type, constraint) { }

    /// Returns the type of this integer subtype.
    IntegerType *getTypeOf() const {
        return llvm::cast<IntegerType>(SubType::getTypeOf());
    }

    /// Returns the constraint of this subtype, or null if this subtype is not
    /// constrained.
    RangeConstraint *getConstraint() const {
        return llvm::cast<RangeConstraint>(SubTypeConstraint);
    }

    static bool classof(const IntegerSubType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerSubType;
    }
};

} // End comma namespace

#endif
