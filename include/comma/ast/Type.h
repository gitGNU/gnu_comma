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
#include "llvm/ADT/GraphTraits.h"
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

    ModelDecl *getDeclaration() const {
        return declaration;
    }

    virtual IdentifierInfo *getIdInfo() const;

    const char *getString() const {
        return getIdInfo()->getString();
    }

    bool isParameterized() const { return arity != 0; }

    unsigned getNumArgs() const { return arity; }

    ModelType *getArgument(unsigned i) const { return arguments[i]; }

    typedef ModelType **arg_iterator;
    arg_iterator beginArguments() const { return &arguments[0]; }
    arg_iterator endArguments()   const { return &arguments[arity]; }

    virtual bool has(SignatureType *type) = 0;

    static bool classof(const ModelType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelType();
    }

protected:
    ModelType(AstKind kind, ModelDecl *decl)
        : Type(kind), declaration(decl), arity(0) {
        assert(this->denotesModelType());
    }

    ModelType(AstKind kind, ModelDecl *decl,
              ModelType **args, unsigned numArgs);

    ModelDecl *declaration;
    ModelType **arguments;
    unsigned arity;
};

//===----------------------------------------------------------------------===//
// SignatureType

class SignatureType : public ModelType, public llvm::FoldingSetNode {

public:
    Sigoid *getDeclaration() const;
    SignatureDecl *getSignature() const;
    VarietyDecl *getVariety() const;

    // Returns true if the given signature is equal to some supersignature of
    // this type.
    bool has(SignatureType *type);

private:
    // FIXME: Perhaps this should be a pointer.  If this type is not
    // parametrized then the direct supersignatures are identical to those of
    // the declaration (and indeed, even for parameterized types, the
    // supersignatures may not be dependent on a formal parameter), and so we
    // could save space by referencing the declarations vector directly whenever
    // possible.
    typedef std::vector<SignatureType*> SuperSigTable;
    SuperSigTable directSupers;

public:
    typedef SuperSigTable::const_iterator sig_iterator;
    sig_iterator beginDirectSupers() const { return directSupers.begin(); }
    sig_iterator endDirectSupers()   const { return directSupers.end(); }

    void  addSupersignature(SignatureType *sig);

    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], arity);
    }

    // Called by SignatureDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, ModelType **args, unsigned numArgs);

    static bool classof(const SignatureType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureType;
    }

private:
    friend class SignatureDecl;
    friend class VarietyDecl;
    friend class llvm::GraphTraits<SignatureType>;

    SignatureType(SignatureDecl *decl);

    SignatureType(VarietyDecl *decl, ModelType **args, unsigned numArgs);

    void populateSignatureTable();

    void insertSupersignature(SignatureType *sig);
};

} // End comma namespace

namespace llvm {

// Specialize GraphTraits to privide access to the full signature hierarchy.
template <>
struct GraphTraits<comma::SignatureType *> {
    typedef comma::SignatureType NodeType;
    typedef comma::SignatureType::sig_iterator ChildIteratorType;

    static NodeType *getEntryNode(comma::SignatureType *sig) {
        return sig;
    }

    static inline ChildIteratorType child_begin(NodeType *node) {
        return node->beginDirectSupers();
    }

    static inline ChildIteratorType child_end(NodeType *node) {
        return node->endDirectSupers();
    }
};

} // End llvm namespace

namespace comma {

//===----------------------------------------------------------------------===//
// DomainType
//
// Base class for all domain types.  Certain domains are characterized only by a
// known signature (formal parameters of signatures and domains), and this base
// provides this perspective.
class DomainType : public ModelType, public llvm::FoldingSetNode {
public:
    // Returns the declaration node defining this type, or NULL if this is an
    // abstract domain.
    Domoid *getDeclaration() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a domain.
    DomainDecl *getDomain() const;

    // Similar to getDeclaration(), but returns non-NULL iff the underlying
    // definition is a functor.
    FunctorDecl *getFunctor() const;

    // Returns the signature describing the shape of this domain.
    SignatureType *getSignature() const;

    // Returns true if this domain is an abstract domain -- that is, the only
    // information known regarding this domain is the signature which it
    // satisfies.
    bool isAbstract() const;

    // The inverse of isAbstract().
    bool isConcrete() const { return !isAbstract(); }

    // Returns true if this domain implements the given signature type.
    bool has(SignatureType *type);

    // Required by the FoldingSet implementation.
    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], arity);
    }

    static void
    Profile(llvm::FoldingSetNodeID &id, ModelType **args, unsigned numArgs);

    // Support isa and dyn_cast.
    static bool classof(const DomainType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainType;
    }

private:
    friend class DomainDecl;
    friend class FunctorDecl;

    // Creates a non-parameterized domain type representing the given domain
    // declaration.
    DomainType(DomainDecl *decl);

    // Create a concrete domain type parameterized over the given arguments.
    DomainType(FunctorDecl *decl, ModelType **args, unsigned numArgs);
};

//===----------------------------------------------------------------------===//
// PercentType
class PercentType : public ModelType {

public:
    PercentType(ModelDecl *decl) : ModelType(AST_PercentType, decl) { }

    IdentifierInfo *getIdInfo() const;

    bool has(SignatureType *type);

    static bool classof(const PercentType *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PercentType;
    }
};

} // End comma namespace

#endif
