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

    virtual Type *getType() const = 0;

    // Sets the declarative region for this decl.  This function can only be
    // called once to initialize the decl.
    void setDeclarativeRegion(DeclarativeRegion *region) {
        assert(context == 0 && "Cannot reset a decl's declarative region!");
        context = region;
    }

    // Returns true if this decl was declared in the given region.
    bool isDeclarativeRegion(DeclarativeRegion *region) {
        return region == context;
    }

    /// Returns this cast to a DeclarativeRegion, or NULL if this model does not
    /// support declarations.
    DeclarativeRegion *asDeclarativeRegion();

    // Support isa and dyn_cast.
    static bool classof(const Decl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDecl();
    }

protected:
    Decl(AstKind kind, IdentifierInfo *info = 0)
        : Ast(kind),
          idInfo(info),
          context(0) {
        assert(this->denotesDecl());
    }

    IdentifierInfo    *idInfo;
    DeclarativeRegion *context;
};

//===----------------------------------------------------------------------===//
// DeclarativeRegion
class DeclarativeRegion {

protected:
    DeclarativeRegion()
        : parent(0) { }

    DeclarativeRegion(DeclarativeRegion *parent)
        : parent(parent) { }

    // FIXME: This datastructure is only temporary.  A better structure is
    // needed.
    typedef std::multimap<IdentifierInfo*, Decl*> DeclarationTable;
    DeclarationTable declarations;

public:
    DeclarativeRegion *getParent() { return parent; }
    const DeclarativeRegion *getParent() const { return parent; }

    // Sets the parent of this region.  This function can only be called if the
    // parent of this region has not yet been set.
    void setParent(DeclarativeRegion *parentRegion) {
        assert(!parent && "Cannot reset the parent of a DeclarativeRegion!");
        parent = parentRegion;
    }

    void addDecl(Decl *decl) {
        IdentifierInfo *name = decl->getIdInfo();
        declarations.insert(DeclarationTable::value_type(name, decl));
    }

    typedef DeclarationTable::iterator DeclIter;
    DeclIter beginDecls() { return declarations.begin(); }
    DeclIter endDecls()   { return declarations.end(); }

    typedef DeclarationTable::const_iterator ConstDeclIter;
    ConstDeclIter beginDecls() const { return declarations.begin(); }
    ConstDeclIter endDecls()   const { return declarations.end(); }

    typedef std::pair<DeclIter, DeclIter> DeclRange;
    DeclRange findDecls(IdentifierInfo *name) {
        return declarations.equal_range(name);
    }

    Decl *findDecl(IdentifierInfo *name, Type *type);

    Decl *findDirectDecl(IdentifierInfo *name, Type *type);

    // Removes the given decl.  Returns true if the decl existed and was
    // removed, false otherwise.
    bool removeDecl(Decl *decl);

    static bool classof(const Ast *node) {
        switch (node->getKind()) {
        default:
            return false;
        case Ast::AST_DomainDecl:
        case Ast::AST_SignatureDecl:
        case Ast::AST_VarietyDecl:
        case Ast::AST_FunctorDecl:
            return true;
        }
    }

    static bool classof(const DomainDecl    *node) { return true; }
    static bool classof(const SignatureDecl *node) { return true; }
    static bool classof(const VarietyDecl   *node) { return true; }
    static bool classof(const FunctorDecl   *node) { return true; }

private:
    DeclarativeRegion *parent;
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
class ModelDecl : public Decl {

public:
    virtual ~ModelDecl() { }

    // Access the location info of this node.
    Location getLocation() const { return location; }

    virtual ModelType *getType() const = 0;

    // Returns true if this model is parameterized.
    bool isParameterized() const {
        return kind == AST_VarietyDecl || kind == AST_FunctorDecl;
    }

    DomainType *getPercent() const { return percent; }

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
    DomainType *percent;
};

//===----------------------------------------------------------------------===//
// Sigoid
//
// This is the common base class for "signature like" objects: i.e. signatures
// and varieties.
class Sigoid : public ModelDecl, public DeclarativeRegion {

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
    VarietyDecl(IdentifierInfo *percentId,
                DomainType    **formals,
                unsigned        arity);

    // Creates a VarietyDecl with the given name, location of definition, and
    // list of AbstractDomainTypes which serve as the formal parameters.
    VarietyDecl(IdentifierInfo *percentId,
                IdentifierInfo *name,
                Location        loc,
                DomainType    **formals,
                unsigned        arity);

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
    DomainType *getFormalDomain(unsigned i) const {
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

    // Returns non-null if this domoid is a FunctorDecl.
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
class DomainDecl : public Domoid, public DeclarativeRegion {

public:
    DomainDecl(IdentifierInfo *percentId,
               IdentifierInfo *name,
               const Location &loc);

    DomainType *getCorrespondingType() { return canonicalType; }
    DomainType *getType() const { return canonicalType; }

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
    DomainType    *canonicalType;
    SignatureDecl *principleSignature;
};

//===----------------------------------------------------------------------===//
// FunctorDecl
//
// Representation of parameterized domains.
class FunctorDecl : public Domoid, public DeclarativeRegion {

public:
    FunctorDecl(IdentifierInfo *percentId,
                IdentifierInfo *name,
                Location        loc,
                DomainType    **formals,
                unsigned        arity);

    // Returns the type node corresponding to this functor applied over the
    // given arguments.  Such types are memorized.  For a given set of arguments
    // this function always returns the same type.
    DomainType *getCorrespondingType(DomainType **args, unsigned numArgs);

    VarietyDecl *getPrincipleSignature() const { return principleSignature; }

    // Returns the type of this functor.
    FunctorType *getFunctorType() const { return functor; }
    FunctorType *getType() const { return functor; }

    // Returns the number of arguments this functor accepts.
    unsigned getArity() const { return getFunctorType()->getArity(); }

    DomainType *getFormalDomain(unsigned i) const {
        return getFunctorType()->getFormalDomain(i);
    }

    // Support for isa and dyn_cast.
    static bool classof(const FunctorDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorDecl;
    }

private:
    mutable llvm::FoldingSet<DomainType> types;

    FunctorType *functor;
    VarietyDecl *principleSignature;
};

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
class AbstractDomainDecl : public Domoid
{
public:
    AbstractDomainDecl(IdentifierInfo *name,
                       SignatureType  *type,
                       Location        loc);

    DomainType *getType() const { return abstractType; }

    SignatureType *getSignatureType() const { return signature; }

    static bool classof(const AbstractDomainDecl *node) { return true; }
    static bool classof(const Ast* node) {
        return node->getKind() == AST_AbstractDomainDecl;
    }

private:
    DomainType    *abstractType;
    SignatureType *signature;
};

//===----------------------------------------------------------------------===//
// FunctionDecl
//
// Representation of function declarations.
class FunctionDecl : public Decl {

public:
    FunctionDecl(IdentifierInfo    *name,
                 FunctionType      *type,
                 Location           loc);

    // Accessors and forwarding functions to the underlying FuntionType node.
    FunctionType *getType() const { return ftype; }

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

    Location getLocation() const { return location; }

    void setBaseDeclaration(FunctionDecl *fdecl) {
        assert(baseDeclaration == 0 && "Cannot reset base declaration!");
        baseDeclaration = fdecl;
    }

    FunctionDecl *getBaseDeclaration() { return baseDeclaration; }
    const FunctionDecl *getBaseDeclaration() const { return baseDeclaration; }

    // Support for isa and dyn_cast.
    static bool classof(const FunctionDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionDecl;
    }

private:
    FunctionType      *ftype;
    Location           location;

    FunctionDecl *baseDeclaration;
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
