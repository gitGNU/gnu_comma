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

    /// Returns true if this type denotes a scalar type.
    bool isScalarType() const;

    /// Returns true if this type denotes an integer type.
    bool isIntegerType() const;

    virtual bool equals(const Type *type) const;

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
};

//===----------------------------------------------------------------------===//
// NamedType
//
// This class represents types that provide IdentifierInfo's which name them.
class NamedType : public Type {

public:
    virtual ~NamedType() { }

    NamedType(AstKind kind, IdentifierInfo *idInfo)
        : Type(kind), idInfo(idInfo) { }

    /// Returns the IdentifierInfo naming this type.
    IdentifierInfo *getIdInfo() const { return idInfo; }

    /// Returns a C-style string naming this type.
    const char *getString() const { return idInfo->getString(); }

    /// Support isa and dyn_cast.
    static bool classof(const NamedType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesNamedType();
    }

private:
    IdentifierInfo *idInfo;
};

//===----------------------------------------------------------------------===//
// CarrierType
//
// The type of carrier declarations.  In the future this node could be combined
// into a general "type alias" node or similar.
class CarrierType : public NamedType {

public:
    CarrierType(CarrierDecl *carrier);

    /// Returns the underlying carrier declaration supporting this type.
    CarrierDecl *getDeclaration();

    /// Returns the representation type which this carrier aliases.
    Type *getRepresentationType();
    const Type *getRepresentationType() const;

    bool equals(const Type *type) const;

    static bool classof(const CarrierType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_CarrierType;
    }

private:
    CarrierDecl *declaration;
};

//===----------------------------------------------------------------------===//
// DomainType
class DomainType : public NamedType {

private:
    /// Domain types are created and owned by a unique DomainTypeDecl.
    friend class DomainTypeDecl;

    /// Creates a type representing the given domain type declaration.
    DomainType(DomainTypeDecl *DTDecl);

public:
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

    /// Returns true if this type and the given type are equal.
    bool equals(const Type *type) const;

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
//
// Ownership of an enumeration type is always deligated to the corresponding
// declaration.
class EnumerationType : public NamedType
{
public:
    EnumerationType(EnumerationDecl *decl);

    Decl *getDeclaration();

    EnumerationDecl *getEnumerationDecl() { return correspondingDecl; }
    const EnumerationDecl *getEnumerationDecl() const {
        return correspondingDecl;
    }

    bool equals(const Type *type) const;

    static bool classof(const EnumerationType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumerationType;
    }

private:
    EnumerationDecl *correspondingDecl;
};

//===----------------------------------------------------------------------===//
// IntegerType
//
// These nodes represent ranged, signed, integer types.  They are allocated,
// owned, and uniqued by an AstResource instance.
//
// NOTE: IntegerType's are constructed with respect to a range, represented by
// llvm::APInt's.  These values must be of identical width, as the bounds of an
// IntegerType must be compatable with the type itself.
class IntegerType : public Type, public llvm::FoldingSetNode
{
public:
    const llvm::APInt &getLowerBound() const { return low; }
    const llvm::APInt &getUpperBound() const { return high; }

    // Returns the number of bits needed to represent this integer type.
    unsigned getBitWidth() const { return low.getBitWidth(); }

    /// Profile implementation for use by llvm::FoldingSet.
    void Profile(llvm::FoldingSetNodeID &ID) {
        return Profile(ID, low, high);
    }

    /// Support isa and dyn_cast;
    static bool classof(const IntegerType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerType;
    }

private:
    // Private constructor used by AstResource to allocate ranged integer types.
    IntegerType(const llvm::APInt &low, const llvm::APInt &high);

    // Profile method used by AstResource to unique integer type nodes.
    static void Profile(llvm::FoldingSetNodeID &ID,
                        const llvm::APInt &low, const llvm::APInt &high);

    friend class AstResource;

    // Lower and upper bounds for this type.
    llvm::APInt low;
    llvm::APInt high;
};

//===----------------------------------------------------------------------===//
// TypedefType
//
// Nodes representing new named types.  These nodes always correspond to a
// declaration in the source code (or a programmaticly generated decl in the
// case of primitive types).  These nodes are owned by the associated
// declaration nodes.
class TypedefType : public NamedType {
public:
    TypedefType(Type *baseType, Decl *decl);

    /// Returns the declaration associated with this type definition.
    Decl *getDeclaration();

    /// Returns the base type of this type definition.
    Type *getBaseType() { return baseType; }
    const Type *getBaseType() const { return baseType; }

    /// Support isa and dyn_cast.
    static bool classof(const TypedefType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_TypedefType;
    }

private:
    Type *baseType;
    Decl *declaration;
};

} // End comma namespace

#endif
