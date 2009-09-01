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
// SignatureType

class SignatureType : public NamedType, public llvm::FoldingSetNode {

public:
    Sigoid *getSigoid() { return declaration; }
    const Sigoid *getSigoid() const { return declaration; }

    SignatureDecl *getSignature() const;

    VarietyDecl *getVariety() const;

    /// Returns true if this type represents an instance of some variety.
    bool isParameterized() const { return getVariety() != 0; }

    /// Returns the number of arguments used to define this type.  When the
    /// supporting declaration is a signature, the arity is zero.
    unsigned getArity() const;

    /// Returns the i'th actual parameter.  This method asserts if its argument
    /// is out of range.
    Type *getActualParameter(unsigned n) const;

    typedef Type **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    /// For use by llvm::FoldingSet.
    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    static bool classof(const SignatureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureType;
    }

private:
    friend class SignatureDecl;
    friend class VarietyDecl;

    SignatureType(SignatureDecl *decl);

    SignatureType(VarietyDecl *decl, Type **args, unsigned numArgs);

    // Called by VarietyDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, Type **args, unsigned numArgs);

    // The declaration supporing this type.
    Sigoid *declaration;

    // If the supporting declaration is a variety, then this array contains the
    // actual arguments defining this instance.
    Type **arguments;
};

//===----------------------------------------------------------------------===//
// DomainType
class DomainType : public NamedType, public llvm::FoldingSetNode {

public:
    /// Creates a domain type representing the given domain value declaration.
    DomainType(DomainValueDecl *DVDecl);

    // Creates a domain type representing the % node of the given model.
    static DomainType *getPercent(IdentifierInfo *percentInfo,
                                  ModelDecl *model);

    // Returns true if this node is a percent node.
    bool denotesPercent() const;

    /// Returns the declaration associated with this domain type.  This can be
    /// either a DomainValueDecl or a ModelDecl.  In the latter case, this
    /// domain type represents the type of % within the context of of the model.
    Decl *getDeclaration() { return declaration; }
    const Decl *getDeclaration() const { return declaration; }

    ModelDecl *getModelDecl() const;

    DomainValueDecl *getDomainValueDecl() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a domain instance declaration.
    DomainInstanceDecl *getInstanceDecl() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is an abstract domain.
    AbstractDomainDecl *getAbstractDecl() const;

    // Returns true if the underlying declaration is an AbstractDomainDecl.
    bool isAbstract() const { return getAbstractDecl() != 0; }

    // Returns true if this type and the given type are equal.
    bool equals(const Type *type) const;

    // Support isa and dyn_cast.
    static bool classof(const DomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainType;
    }

private:
    // This constructor is called by getPercent() to create a percent node.
    DomainType(IdentifierInfo *percentId, ModelDecl *model);

    Decl *declaration;
};

//===----------------------------------------------------------------------===//
// SubroutineType
class SubroutineType : public Type {

protected:
    // This constructor produces a subroutine type where the parameter modes are
    // set to MODE_DEFAULT.
    SubroutineType(AstKind kind,
                   IdentifierInfo **formals,
                   Type **argTypes,
                   unsigned numArgs);

    // Constructor where each parameter mode can be specified.
    SubroutineType(AstKind kind,
                   IdentifierInfo **formals,
                   Type **argTypes,
                   PM::ParameterMode *modes,
                   unsigned numArgs);

public:
    // Returns the number of arguments accepted by this type.
    unsigned getArity() const { return numArgs; }

    // Returns the type of the i'th parameter.
    Type *getArgType(unsigned i) const;

    // Returns the i'th keyword for this type.
    IdentifierInfo *getKeyword(unsigned i) const {
        assert(i < getArity() && "Index out of range!");
        return keywords[i];
    }

    int getKeywordIndex(IdentifierInfo *key) const;

    // Returns the i'th parameter mode for this type.  Parameters with
    // MODE_DEFAULT are automatically converted to MODE_IN (if this conversion
    // is undesierable use getExplicitParameterMode instead).
    PM::ParameterMode getParameterMode(unsigned i) const;

    // Returns the i'th parameter mode for this type.
    PM::ParameterMode getExplicitParameterMode(unsigned i) const;

    // Sets the i'th parameter mode.  This method will assert if this subroutine
    // denotes a function type and the mode is `out' or `in out'.
    void setParameterMode(PM::ParameterMode mode, unsigned i);

    // Returns an array of IdentifierInfo's corresponding to the keyword set for
    // this type, or 0 if there are no parameters.  This function is intended to
    // be used to simplify construction of new SubroutineType nodes, not as
    // general purpose accessor.
    IdentifierInfo **getKeywordArray() const;

    // Returns true if the keywords of the given type match exactly those of
    // this type.  The arity of both subroutine types must match for this
    // function to return true.
    bool keywordsMatch(const SubroutineType *routineType) const;

    // Returns true if this type is equal to the given subroutine type.  Both
    // this type and the target must both be function or procedure types, the
    // arity, argument, and (in the case of functions) the return types must
    // match.  Actual argument keywords are not considered when testing for
    // equality.
    bool equals(const Type *type) const;

    // Support isa and dyn_cast.
    static bool classof(const SubroutineType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubroutineType();
    }

private:
    // We munge the supplied parameter type pointers and store the mode
    // associations in the lower two bits.
    typedef llvm::PointerIntPair<Type*, 2> ParamInfo;

    IdentifierInfo **keywords;
    ParamInfo       *parameterInfo;
    unsigned         numArgs;
};

//===----------------------------------------------------------------------===//
// FunctionType
class FunctionType : public SubroutineType {

public:
    FunctionType(IdentifierInfo **formals,
                 Type           **argTypes,
                 unsigned         numArgs,
                 Type            *returnType)
        : SubroutineType(AST_FunctionType, formals, argTypes, numArgs),
          returnType(returnType) { }

    // Returns the result type of this function.
    Type *getReturnType() const { return returnType; }

    // Support isa and dyn_cast.
    static bool classof(const FunctionType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionType;
    }

private:
    Type *returnType;
};

//===----------------------------------------------------------------------===//
// ProcedureType
class ProcedureType : public SubroutineType {

public:
    ProcedureType(IdentifierInfo **formals,
                  Type           **argTypes,
                  unsigned         numArgs)
        : SubroutineType(AST_ProcedureType, formals, argTypes, numArgs) { }

    // Support isa and dyn_cast.
    static bool classof(const ProcedureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureType;
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
