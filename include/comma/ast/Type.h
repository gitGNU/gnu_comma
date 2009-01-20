//===-- ast/Type.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_TYPE_HDR_GUARD
#define COMMA_AST_TYPE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/AstRewriter.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Casting.h"
#include <vector>

namespace comma {

//===----------------------------------------------------------------------===//
// Type

class Type : public Ast {

public:
    virtual ~Type() { }

    static bool classof(const Type *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesType();
    }

protected:
    Type(AstKind kind) : Ast(kind) {
        assert(this->denotesType());
    }
};

//===----------------------------------------------------------------------===//
// ModelType

class ModelType : public Type {

public:
    virtual ~ModelType() { }

    IdentifierInfo *getIdInfo() const { return idInfo; }

    // Returns a c-string representing the name of this model, or NULL if this
    // model is anonymous.
    const char *getString() const {
        return getIdInfo()->getString();
    }

    // Suport isa and dyn_cast.
    static bool classof(const ModelType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelType();
    }

protected:
    ModelType(AstKind kind, IdentifierInfo *idInfo)
        : Type(kind),
          idInfo(idInfo) { assert(this->denotesModelType()); }

    IdentifierInfo *idInfo;
};

//===----------------------------------------------------------------------===//
// SignatureType

class SignatureType : public ModelType, public llvm::FoldingSetNode {

public:
    Sigoid *getDeclaration() const;

    SignatureDecl *getSignature() const;

    VarietyDecl *getVariety() const;

    // Returns true if this type is a variety instance.
    bool isParameterized() const { return getVariety() != 0; }

    // Returns the number of arguments used to define this type.  When the
    // supporting declaration is a signature, the arity is zero.
    unsigned getArity() const;

    // Returns the i'th actual parameter.  This function asserts if its argument
    // is out of range.
    DomainType *getActualParameter(unsigned n) const;

    typedef DomainType **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    // Called by VarietyDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, DomainType **args, unsigned numArgs);

    static bool classof(const SignatureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureType;
    }

private:
    friend class SignatureDecl;
    friend class VarietyDecl;

    SignatureType(SignatureDecl *decl);

    SignatureType(VarietyDecl *decl, DomainType **args, unsigned numArgs);

    // The underlying signature decl.
    Sigoid *sigoid;

    // If the supporting declaration is a variety, then this array contains the
    // actual arguments defining this instance.
    DomainType **arguments;
};

//===----------------------------------------------------------------------===//
// ParameterizedType
//
// Base class for both functor and variety types.

class ParameterizedType : public ModelType {

public:
    virtual ~ParameterizedType() { }

    unsigned getArity() const { return numFormals; }

    // Returns the abstract domain representing the formal parameter.
    AbstractDomainType *getFormalDomain(unsigned i) const;

    // Returns the SignatureType which the formal parameter satisfies (or which
    // an actual parameter must satisfy).
    SignatureType *getFormalType(unsigned i) const;

    // Returns the IdentifierInfo which labels this formal parameter.
    IdentifierInfo *getFormalIdInfo(unsigned i) const;

    // Returns the index of the parameter corresponding to the named selector,
    // or -1 if no such selector exists.
    int getSelectorIndex(IdentifierInfo *selector) const;

    static bool classof(const ParameterizedType *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_VarietyType || kind == AST_FunctorType;
    }

protected:
    ParameterizedType(AstKind              kind,
                      IdentifierInfo      *idInfo,
                      AbstractDomainType **formalArguments,
                      unsigned             arity);

    AbstractDomainType **formals;
    unsigned numFormals;
};

//===----------------------------------------------------------------------===//
// VarietyType
//
// These nodes represent the type of a parameterized signature.  In some sense,
// they do not represent real types -- they are incomplete until provided with a
// compatible set of actual arguments.  The main role of these types is to
// provide a handle for the purpose of lookup resolution.
//
// VarietyType nodes are always owned by their associated decl.
class VarietyType : public ParameterizedType {

public:
    VarietyType(AbstractDomainType **formalArguments,
                VarietyDecl *variety, unsigned arity);

    ~VarietyType();

    VarietyDecl *getVarietyDecl() const { return variety; }

    static bool classof(const VarietyType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_VarietyType;
    }

private:
    friend class VarietyDecl;

    VarietyDecl *variety;
};

//===----------------------------------------------------------------------===//
// FunctorType
//
// These nodes represent the type of a parameterized domain and serve
// essentially the same purpose of VarietyType nodes.  Again, FunctorType's do
// not represent real types (they are incomplete until provided with a
// compatible set of actual arguments), and are owned by their associated decl.
class FunctorType : public ParameterizedType {

public:
    FunctorType(AbstractDomainType **formalArguments,
                FunctorDecl *functor, unsigned arity);

    ~FunctorType();

    FunctorDecl *getFunctorDecl() const { return functor; }

    static bool classof(const FunctorType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorType;
    }

private:
    FunctorDecl *functor;
};

//===----------------------------------------------------------------------===//
// DomainType
//
// Base class for all domain types.  Certain domains are characterized only by a
// known signature (formal parameters of signatures and domains, for example),
// others are concrete types with an associated declaration node.  The former
// are called "abstract domains", the latter "concrete".
class DomainType : public ModelType {

public:
    virtual ~DomainType() { }

    // Returns the declaration node defining this type. This method is valid for
    // all domain types.
    ModelDecl *getDeclaration() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a domoid.
    Domoid *getDomoid() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a domain.
    DomainDecl *getDomain() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a functor.
    FunctorDecl *getFunctor() const;

    // Returns true if this domain is an abstract domain -- that is, the only
    // information known regarding this domain is the signature which it
    // satisfies.
    bool isAbstract() const;

    // The inverse of isAbstract().
    bool isConcrete() const { return !isAbstract(); }

    ConcreteDomainType *getConcreteType();

    // Support isa and dyn_cast.
    static bool classof(const DomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDomainType();
    }

protected:
    DomainType(AstKind kind, IdentifierInfo *idInfo, ModelDecl *decl);
    DomainType(AstKind kind, IdentifierInfo *idInfo, SignatureType *sig);

    union {
        Ast           *astNode;
        ModelDecl     *modelDecl;
        SignatureType *signatureType;
    };
};

//===----------------------------------------------------------------------===//
// ConcreteDomainType
//
// These types are always owned by the declaration nodes which define them.
class ConcreteDomainType : public DomainType, public llvm::FoldingSetNode {

public:
    // Returns the number of arguments used to define this type.  When the
    // supporting declaration is a domain, the arity is zero.  When the
    // supporting declaration is a functor, this method returns the number of
    // actual parameters.
    unsigned getArity() const;

    // Returns the i'th actual parameter.  This function asserts if its argument
    // is out of range,
    DomainType *getActualParameter(unsigned n) const;

    // Returns true if this domain type is a functor instance.
    bool isParameterized() const { return arguments != 0; }

    typedef DomainType **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    // Called by FunctorDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, DomainType **args, unsigned numArgs);

    // Support isa and dyn_cast.
    static bool classof(const ConcreteDomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ConcreteDomainType;
    }

private:
    friend class DomainDecl;
    friend class FunctorDecl;

    // Creates a domain type representing the given domain declaration.
    ConcreteDomainType(DomainDecl *decl);

    // Creates a domain type representing an instance of the given functor
    // declaration.
    ConcreteDomainType(FunctorDecl *decl, DomainType **args, unsigned numArgs);

    // If the supporting domain is a functor, then this array contains the
    // actual arguments defining this instance.
    DomainType **arguments;
};

//===----------------------------------------------------------------------===//
// AbstractDomainType
class AbstractDomainType : public DomainType {

public:
    // Creates an abstract domain type which satisfies the given signature.
    AbstractDomainType(IdentifierInfo *name, SignatureType *signature)
        : DomainType(AST_AbstractDomainType, name, signature),
          idInfo(name) { };

    // Returns the signature type describing this abstract domain.
    SignatureType *getSignature() const { return signatureType; }

    // Support isa and dyn_cast.
    static bool classof(const AbstractDomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AbstractDomainType;
    }

private:
    IdentifierInfo *idInfo;
};

//===----------------------------------------------------------------------===//
// PercentType
class PercentType : public DomainType {

public:
    PercentType(IdentifierInfo *idInfo, ModelDecl *decl)
        : DomainType(AST_PercentType, idInfo, decl) { }

    static bool classof(const PercentType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PercentType;
    }
};

// Inline functions mapping DomainType nodes to its base classes.
inline ConcreteDomainType *DomainType::getConcreteType()
{
    return llvm::dyn_cast<ConcreteDomainType>(this);
}


//===----------------------------------------------------------------------===//
// FunctionType
class FunctionType : public Type {

public:
    FunctionType(IdentifierInfo **formals,
                 DomainType     **argTypes,
                 unsigned         numArgs,
                 DomainType      *returnType);

    // Returns the number of arguments accepted by this type.
    unsigned getArity() const { return numArgs; }

    // Returns the result type of this function.
    DomainType *getReturnType() const { return returnType; }

    // Returns the type of the i'th parameter.
    DomainType *getArgType(unsigned i) const {
        assert(i < getArity() && "Index out of range!");
        return argumentTypes[i];
    }

    // Returns the i'th selector for this type.
    IdentifierInfo *getSelector(unsigned i) const {
        assert(i < getArity() && "Index out of range!");
        return selectors[i];
    }

    // Returns true if the selectors of the given type match exactly those of
    // this type.  The arity of both function types must match for this function
    // to return true.
    bool selectorsMatch(const FunctionType *ftype) const;

    // Returns true if this type is equal to ftype.  Equality in this case is
    // determined by the arity, argument types and return types (the argument
    // selectors are not considered in this case).
    bool equals(const FunctionType *ftype) const;

private:
    IdentifierInfo **selectors;
    DomainType     **argumentTypes;
    DomainType      *returnType;
    unsigned         numArgs;
};

} // End comma namespace

#endif
