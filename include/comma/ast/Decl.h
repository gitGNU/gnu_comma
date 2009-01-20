//===-- ast/Decl.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECL_HDR_GUARD
#define COMMA_AST_DECL_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <map>

namespace comma {

//===----------------------------------------------------------------------===//
// Decl.
//
// Decl nodes represent declarations within a Comma program.
class Decl : public Ast {

public:
    virtual ~Decl() { };

    // Returns the IdentifierInfo object associated with this decl, or NULL if
    // this is an anonymous decl.
    IdentifierInfo *getIdInfo() const { return idInfo; }

    // Returns the name of this decl as a c string, or NULL if this is an
    // anonymous decl.
    const char *getString() const {
        return idInfo ? idInfo->getString() : 0;
    }

    // Returns true if this decl is anonymous.  Currently, the only anonymous
    // models are the "principle signatures" of a domain.
    bool isAnonymous() const { return idInfo == 0; }

    // Support isa and dyn_cast.
    static bool classof(const Decl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDecl();
    }

protected:
    Decl(AstKind kind, IdentifierInfo *info = 0) : Ast(kind), idInfo(info) {
        assert(this->denotesDecl());
    }

    IdentifierInfo *idInfo;
};

//===----------------------------------------------------------------------===//
// TypeDecl
//
// The root of the comma type hierarchy,
class TypeDecl : public Decl {

public:
    virtual ~TypeDecl() { }

    // Support isa and dyn_cast.
    static bool classof(const TypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesTypeDecl();
    }

protected:
    TypeDecl(AstKind kind, IdentifierInfo *info = 0)
        : Decl(kind, info) {
        assert(this->denotesTypeDecl());
    }
};

//===----------------------------------------------------------------------===//
// ModelDecl
//
// Models represent those attributes and characteristics which both signatures
// and domains share.
//
// The constructors of a ModelDecl (and of all sub-classes) take an
// IdentifierInfo node as a parameter which is asserted to resolve to "%".  This
// additional parameter is necessary since the Ast classes cannot create
// IdentifierInfo's on their own -- we do not have access to a global
// IdentifierPool with which to create them.
class ModelDecl : public TypeDecl {

public:
    virtual ~ModelDecl() { }

    // Access the location info of this node.
    Location getLocation() const { return location; }

    // Returns true if this model is parameterized.
    bool isParameterized() const {
        return kind == AST_VarietyDecl || kind == AST_FunctorDecl;
    }

    PercentType *getPercent() const { return percent; }

    // Returns the type of this model.
    virtual ModelType *getType() const = 0;

    // Support isa and dyn_cast.
    static bool classof(const ModelDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelDecl();
    }

protected:
    // Creates an anonymous Model.  Note that anonymous models do not have valid
    // location information as they never correspond to a source level construct.
    ModelDecl(AstKind kind, IdentifierInfo *percentId);

    ModelDecl(AstKind         kind,
              IdentifierInfo *percentId,
              IdentifierInfo *name,
              const Location &loc);

    // Location information provided by the constructor.
    Location location;

    // Percent node for this decl.
    PercentType *percent;
};

//===----------------------------------------------------------------------===//
// Sigoid
//
// This is the common base class for "signature like" objects: i.e. signatures
// and varieties.
class Sigoid : public ModelDecl {

public:
    Sigoid(AstKind kind, IdentifierInfo *percentId)
        : ModelDecl(kind, percentId) { }

    Sigoid(AstKind         kind,
           IdentifierInfo *percentId,
           IdentifierInfo *idInfo,
           Location        loc)
        : ModelDecl(kind, percentId, idInfo, loc) { }

    virtual ~Sigoid() { }

    // If this is a SignatureDecl, returns this cast to the refined type,
    // otherwise returns NULL.
    SignatureDecl *getSignature();

    // If this is a VarietyDecl, returns this cast to the refined type,
    // otherwise returns NULL.
    VarietyDecl *getVariety();

protected:
    typedef llvm::SmallPtrSet<SignatureType*, 8> SignatureTable;
    SignatureTable directSupers;
    SignatureTable supersignatures;

public:
    // Adds a direct super signature.
    void addSupersignature(SignatureType *supersignature);

    typedef SignatureTable::const_iterator sig_iterator;
    sig_iterator beginDirectSupers() const { return directSupers.begin(); }
    sig_iterator endDirectSupers()   const { return directSupers.end(); }

    sig_iterator beginSupers() const { return supersignatures.begin(); }
    sig_iterator endSupers()   const { return supersignatures.end(); }

protected:
    typedef std::multimap<IdentifierInfo*, FunctionDecl*> ComponentTable;
    ComponentTable components;

public:
    typedef ComponentTable::iterator ComponentIter;
    ComponentIter beginComponents() { return components.begin(); }
    ComponentIter endComponents()   { return components.end(); }

    // Adds a declaration to the set of components for this sigoid.
    void addComponent(FunctionDecl *fdecl);

    FunctionDecl *findComponent(IdentifierInfo *name,
                                FunctionType *ftype);

    FunctionDecl *findDirectComponent(IdentifierInfo *name,
                                      FunctionType *ftype);


    typedef std::pair<ComponentIter, ComponentIter> ComponentRange;
    ComponentRange findComponents(IdentifierInfo *name) {
        return components.equal_range(name);
    }

    // Removes the given component from this sigoid.  Returns true if the given
    // function decl existed and was successfully removed.
    bool removeComponent(FunctionDecl *fdecl);

    static bool classof(const Sigoid *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_SignatureDecl || kind == AST_VarietyDecl;
    }
};

//===----------------------------------------------------------------------===//
// SignatureDecl
//
// This class defines (non-parameterized) signature declarations.
class SignatureDecl : public Sigoid {

public:
    // Creates an anonymous signature.
    SignatureDecl(IdentifierInfo *percentId);
    SignatureDecl(IdentifierInfo *percentId,
                  IdentifierInfo *name,
                  const Location &loc);

    SignatureType *getCorrespondingType() { return canonicalType; }
    SignatureType *getType() const { return canonicalType; }

    // Support for isa and dyn_cast.
    static bool classof(const SignatureDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureDecl;
    }

private:
    // The unique type representing this signature (accessible via
    // getCorrespondingType()).
    SignatureType *canonicalType;
};

//===----------------------------------------------------------------------===//
// VarietyDecl
//
// Repesentation of parameterized signatures.
class VarietyDecl : public Sigoid {

public:
    // Creates an anonymous variety.
    VarietyDecl(IdentifierInfo      *percentId,
                AbstractDomainType **formals,
                unsigned             arity);

    // Creates a VarietyDecl with the given name, location of definition, and
    // list of AbstractDomainTypes which serve as the formal parameters.
    VarietyDecl(IdentifierInfo      *percentId,
                IdentifierInfo      *name,
                Location             loc,
                AbstractDomainType **formals,
                unsigned             arity);

    // Returns the type node corresponding to this variety applied over the
    // given arguments.
    SignatureType *getCorrespondingType(DomainType **args, unsigned numArgs);
    SignatureType *getCorrespondingType();

    // Returns the type of this variety.
    VarietyType *getVarietyType() const { return varietyType; }
    VarietyType *getType() const { return varietyType; }

    // Returns the number of arguments accepted by this variety.
    unsigned getArity() const { return getVarietyType()->getArity(); }

    // Returns the an abstract domain node representing the i'th formal
    // parameter.
    AbstractDomainType *getFormalDomain(unsigned i) const {
        return getVarietyType()->getFormalDomain(i);
    }

    typedef llvm::FoldingSet<SignatureType>::iterator type_iterator;
    type_iterator beginTypes() { return types.begin(); }
    type_iterator endTypes() { return types.end(); }

    // Support for isa and dyn_cast.
    static bool classof(const VarietyDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_VarietyDecl;
    }

private:
    mutable llvm::FoldingSet<SignatureType> types;

    VarietyType *varietyType;
};


//===----------------------------------------------------------------------===//
// Domoid
//
// This is the common base class for domain-like objects: i.e. domains
// and functors.
class Domoid : public ModelDecl {

public:
    virtual ~Domoid() { }

    // Returns non-null if this domoid is a DomainDecl.
    DomainDecl *getDomain();

    // Returns non-null if this domoid is a functorDecl.
    FunctorDecl *getFunctor();

    // Each Domoid has an associated signature declaration, known as its
    // "principle signature".  These signatures are unnamed (anonymous) and
    // provide the public view of the domain (its exports and the super
    // signatures it implements).
    SignatureDecl *getPrincipleSignature() const {
        return principleSignature;
    }

    static bool classof(const Domoid *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_DomainDecl || kind == AST_FunctorDecl;
    }

protected:
    Domoid(AstKind         kind,
           IdentifierInfo *percentId,
           IdentifierInfo *idInfo,
           Location        loc);

    SignatureDecl *principleSignature;
};

//===----------------------------------------------------------------------===//
// DomainDecl
//
class DomainDecl : public Domoid {

public:
    DomainDecl(IdentifierInfo *percentId,
               IdentifierInfo *name,
               const Location &loc);

    ConcreteDomainType *getCorrespondingType() { return canonicalType; }
    ConcreteDomainType *getType() const { return canonicalType; }

    SignatureDecl *getPrincipleSignature() const { return principleSignature; }

    // Support for isa and dyn_cast.
    static bool classof(const DomainDecl *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_DomainDecl || kind == AST_FunctorDecl;
    }

protected:
    // For use by subclasses.
    DomainDecl(AstKind         kind,
               IdentifierInfo *percentId,
               IdentifierInfo *info,
               Location        loc);

private:
    ConcreteDomainType *canonicalType;
    SignatureDecl *principleSignature;
};

//===----------------------------------------------------------------------===//
// FunctorDecl
//
// Representation of parameterized domains.
class FunctorDecl : public Domoid {

public:
    FunctorDecl(IdentifierInfo      *percentId,
                IdentifierInfo      *name,
                Location             loc,
                AbstractDomainType **formals,
                unsigned             arity);

    // Returns the type node corresponding to this functor applied over the
    // given arguments.  Such types are memorized.  For a given set of arguments
    // this function always returns the same type.
    ConcreteDomainType *getCorrespondingType(DomainType **args,
                                             unsigned numArgs);

    VarietyDecl *getPrincipleSignature() const { return principleSignature; }

    // Returns the type of this functor.
    FunctorType *getFunctorType() const { return functor; }
    FunctorType *getType() const { return functor; }

    // Returns the number of arguments this functor accepts.
    unsigned getArity() const { return getFunctorType()->getArity(); }

    AbstractDomainType *getFormalDomain(unsigned i) const {
        return getFunctorType()->getFormalDomain(i);
    }

    // Support for isa and dyn_cast.
    static bool classof(const FunctorDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorDecl;
    }

private:
    mutable llvm::FoldingSet<ConcreteDomainType> types;

    FunctorType *functor;
    VarietyDecl *principleSignature;
};

//===----------------------------------------------------------------------===//
// FunctionDecl
//
// Representation of function declarations.
class FunctionDecl : public Decl {

public:
    FunctionDecl(IdentifierInfo   *name,
                 FunctionType     *type,
                 ModelType        *context,
                 Location          loc);

    // Returns true if this function declaration was defined in the context of
    // the given type.
    bool isTypeContext(const ModelType *type) const {
        return type == context;
    }

    // Accessors and forwarding functions to the underlying FuntionType node.
    FunctionType *getFunctionType() const { return ftype; }

    unsigned getArity() const { return ftype->getArity(); }

    IdentifierInfo *getSelector(unsigned i) const {
        return ftype->getSelector(i);
    }

    DomainType *getArgType(unsigned i) const {
        return ftype->getArgType(i);
    }

    DomainType *getReturnType() const {
        return ftype->getReturnType();
    }

    // Support for isa and dyn_cast.
    static bool classof(const FunctionDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionDecl;
    }

private:
    FunctionType *ftype;
    ModelType    *context;
    Location      location;
};

//===----------------------------------------------------------------------===//
// Inline methods, now that the decl hierarchy is in place.

inline SignatureDecl *Sigoid::getSignature()
{
    return llvm::dyn_cast<SignatureDecl>(this);
}

inline VarietyDecl *Sigoid::getVariety()
{
    return llvm::dyn_cast<VarietyDecl>(this);
}

inline DomainDecl *Domoid::getDomain()
{
    return llvm::dyn_cast<DomainDecl>(this);
}

inline FunctorDecl *Domoid::getFunctor()
{
    return llvm::dyn_cast<FunctorDecl>(this);
}

} // End comma namespace

#endif
