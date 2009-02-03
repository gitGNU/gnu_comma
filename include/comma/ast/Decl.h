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

    // Returns the location associated with this decl.
    Location getLocation() const { return location; }

    // Returns true if this decl is anonymous.  Currently, the only anonymous
    // models are the "principle signatures" of a domain.
    bool isAnonymous() const { return idInfo == 0; }

    // Sets the declarative region for this decl.  This function can only be
    // called once to initialize the decl.
    void setDeclarativeRegion(DeclarativeRegion *region) {
        assert(context == 0 && "Cannot reset a decl's declarative region!");
        context = region;
    }

    // Returns the declarative region for this decl.  Sometimes decls are
    // created before their associated regions exist, so this method may return
    // null.
    DeclarativeRegion *getDeclarativeRegion() { return context; }

    // Returns true if this decl was declared in the given region.
    bool isDeclaredIn(DeclarativeRegion *region) {
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
    Decl(AstKind kind, IdentifierInfo *info = 0, Location loc = 0)
        : Ast(kind),
          idInfo(info),
          location(loc),
          context(0) {
        assert(this->denotesDecl());
    }

    IdentifierInfo    *idInfo;
    Location           location;
    DeclarativeRegion *context;
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

    virtual const ModelType *getType() const = 0;

    ModelType *getType() {
        return const_cast<ModelType*>(
            const_cast<const ModelDecl*>(this)->getType());
    }

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
              Location        loc);

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
    // Constructs an anonymous signature.
    Sigoid(AstKind kind, IdentifierInfo *percentId)
        : ModelDecl(kind, percentId),
          DeclarativeRegion(kind) { }

    // This constructor is used to create an anonymous signature which inherits
    // its percent node from a domain.  Used when creating principle signatures.
    Sigoid(AstKind Kind, DomainType *percent);

    // Creates a named signature.
    Sigoid(AstKind         kind,
           IdentifierInfo *percentId,
           IdentifierInfo *idInfo,
           Location        loc)
        : ModelDecl(kind, percentId, idInfo, loc),
          DeclarativeRegion(kind) { }

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

    // Creates a named signature.
    SignatureDecl(IdentifierInfo *percentId,
                  IdentifierInfo *name,
                  const Location &loc);

    // Creates a 'principle signature' for the given domain.
    SignatureDecl(DomainDecl *domain);

    // Creates a 'principle signature' for the given functor.
    SignatureDecl(FunctorDecl *functor);

    // Returns true if this signature is the principle signature of some domain.
    bool isPrincipleSignature() const;

    SignatureType *getCorrespondingType() { return canonicalType; }

    const SignatureType *getType() const { return canonicalType; }
    SignatureType *getType() { return canonicalType; }

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

    const VarietyType *getType() const { return varietyType; }
    VarietyType *getType() { return varietyType; }

    // Returns the number of arguments accepted by this variety.
    unsigned getArity() const { return getType()->getArity(); }

    // Returns the an abstract domain node representing the i'th formal
    // parameter.
    DomainType *getFormalDomain(unsigned i) const {
        return getType()->getFormalDomain(i);
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

    // Returns the principle signature declaration if this domoid admits one,
    // NULL otherwise.  The only domain decl which does not have a principle
    // signature is an AbstractDomainDecl.
    virtual SignatureDecl *getPrincipleSignature() { return 0; }

    // Returns the AddDecl which provides the implementation for this domoid, or
    // NULL if no implementation is available.  The only domain decl which does
    // not provide an implementation is an AbstractDomainDecl.
    virtual const AddDecl *getImplementation() const { return 0; }

    AddDecl *getImplementation() {
        return const_cast<AddDecl*>(
            const_cast<const Domoid*>(this)->getImplementation());
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
};

//===----------------------------------------------------------------------===//
// AddDecl
//
// This class represents an add expression.  It provides a declarative region
// for the body of a domain and contains all function and values which the
// domain defines.
class AddDecl : public Decl, public DeclarativeRegion {

public:
    // Creates an AddDecl to represent the body of the given domain.
    AddDecl(DomainDecl *domain);

    // Creates an AddDecl to represent the body of the given functor.
    AddDecl(FunctorDecl *functor);

    // Returns true if this Add implements a DomainDecl.
    bool implementsDomain() const;

    // Returns true if this Add implements a FunctorDecl.
    bool implementsFunctor() const;

    // If implementsDomain returns true, this function provides the domain
    // declaration which this add implements, otherwise NULL is returned.
    DomainDecl *getImplementedDomain();

    // If implementsFunctor returns true, this function provides the functor
    // declaration which this add implements, otherwise NULL is returned.
    FunctorDecl *getImplementedFunctor();

    static bool classof(AddDecl *node) { return true; }
    static bool classof(Ast *node) {
        return node->getKind() == AST_AddDecl;
    }
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

    const DomainType *getType() const { return canonicalType; }
    DomainType *getType() { return canonicalType; }

    SignatureDecl *getPrincipleSignature() { return principleSignature; }

    // Returns the AddDecl which implements this domain.
    const AddDecl *getImplementation() const { return implementation; }

    // Support for isa and dyn_cast.
    static bool classof(const DomainDecl *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_DomainDecl || kind == AST_FunctorDecl;
    }

private:
    DomainType    *canonicalType;
    SignatureDecl *principleSignature;
    AddDecl       *implementation;
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

    SignatureDecl *getPrincipleSignature() { return principleSignature; }

    // Returns the AddDecl which implements this functor.
    const AddDecl *getImplementation() const { return implementation; }

    // Returns the type of this functor.
    const FunctorType *getType() const { return functor; }
    FunctorType *getType() { return functor; }

    // Returns the number of arguments this functor accepts.
    unsigned getArity() const { return getType()->getArity(); }

    DomainType *getFormalDomain(unsigned i) const {
        return getType()->getFormalDomain(i);
    }

    // Support for isa and dyn_cast.
    static bool classof(const FunctorDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorDecl;
    }

private:
    mutable llvm::FoldingSet<DomainType> types;

    FunctorType   *functor;
    SignatureDecl *principleSignature;
    AddDecl       *implementation;
};

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
class AbstractDomainDecl : public Domoid
{
public:
    AbstractDomainDecl(IdentifierInfo *name,
                       SignatureType  *type,
                       Location        loc);

    const DomainType *getType() const { return abstractType; }
    DomainType *getType() { return abstractType; }

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
// SubroutineDecl
//
// Base class for representing procedures and functions.
class SubroutineDecl : public Decl, public DeclarativeRegion {

protected:
    // When this constructor is invoked, a new type is generated to represent
    // the subroutine.  In addition, the parameter decls are updated so that
    // their associated declarative regions point to the newly constructed decl.
    SubroutineDecl(AstKind            kind,
                   IdentifierInfo    *name,
                   Location           loc,
                   ParamValueDecl   **params,
                   unsigned           numParams,
                   DomainType        *returnType,
                   DeclarativeRegion *parent);

    // This constructor is provided when we need to construct a decl given a
    // type.  In this case, a set of ParamValueDecls are implicitly constructed
    // according to the type provided.
    SubroutineDecl(AstKind            kind,
                   IdentifierInfo    *name,
                   Location           loc,
                   SubroutineType    *type,
                   DeclarativeRegion *parent);

public:
    const SubroutineType *getType() const { return routineType; }
    SubroutineType *getType() { return routineType; }

    unsigned getArity() const { return routineType->getArity(); }

    IdentifierInfo *getKeyword(unsigned i) const {
        return routineType->getKeyword(i);
    }

    DomainType *getArgType(unsigned i) const {
        return routineType->getArgType(i);
    }

    typedef ParamValueDecl **ParamDeclIterator;

    ParamDeclIterator beginParams() { return parameters; }
    ParamDeclIterator endParams()   { return parameters + getArity(); }

    void setBaseDeclaration(SubroutineDecl *routineDecl);
    SubroutineDecl *getBaseDeclaration() { return baseDeclaration; }
    const SubroutineDecl *getBaseDeclaration() const { return baseDeclaration; }

    bool hasBody() const { return body != 0; }

    void setBody(BlockStmt *block) { body = block; }

    BlockStmt *getBody() { return body; }

    const BlockStmt *getBody() const { return body; }

    // Support for isa and dyn_cast.
    static bool classof(const SubroutineDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubroutineDecl();
    }

protected:
    SubroutineType  *routineType;
    SubroutineDecl  *baseDeclaration;
    ParamValueDecl **parameters;
    BlockStmt       *body;
};

//===----------------------------------------------------------------------===//
// FunctionDecl
//
// Representation of function declarations.
class FunctionDecl : public SubroutineDecl {

public:
    FunctionDecl(IdentifierInfo    *name,
                 Location           loc,
                 ParamValueDecl   **params,
                 unsigned           numParams,
                 DomainType        *returnType,
                 DeclarativeRegion *parent)
        : SubroutineDecl(AST_FunctionDecl,
                         name, loc,
                         params, numParams,
                         returnType,
                         parent) { }

    FunctionDecl(IdentifierInfo    *name,
                 Location           loc,
                 FunctionType      *type,
                 DeclarativeRegion *parent)
        : SubroutineDecl(AST_FunctionDecl,
                         name, loc,
                         type, parent) { }

    const FunctionType *getType() const {
        return const_cast<const FunctionType*>(
            const_cast<FunctionDecl*>(this)->getType());
    }

    FunctionType *getType() {
        return llvm::cast<FunctionType>(routineType);
    }

    FunctionDecl *getBaseDeclaration() {
        return llvm::cast_or_null<FunctionDecl>(baseDeclaration);
    }

    const FunctionDecl *getBaseDeclaration() const {
        return const_cast<const FunctionDecl*>(
            const_cast<FunctionDecl*>(this)->getBaseDeclaration());
    }

    DomainType *getReturnType() const {
        return getType()->getReturnType();
    }

    // Support for isa and dyn_cast.
    static bool classof(const FunctionDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctionDecl;
    }
};

//===----------------------------------------------------------------------===//
// ProcedureDecl
//
// Representation of procedure declarations.
class ProcedureDecl : public SubroutineDecl {

public:
    ProcedureDecl(IdentifierInfo    *name,
                  Location           loc,
                  ParamValueDecl   **params,
                  unsigned           numParams,
                  DeclarativeRegion *parent)
        : SubroutineDecl(AST_ProcedureDecl,
                         name, loc,
                         params, numParams,
                         0,     // Null return type for procedures.
                         parent) { }

    const ProcedureType *getType() const {
        return const_cast<const ProcedureType*>(
            const_cast<ProcedureDecl*>(this)->getType());
    }

    ProcedureType *getType() {
        return llvm::cast<ProcedureType>(routineType);
    }

    // Support for isa and dyn_cast.
    static bool classof(const ProcedureDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureDecl;
    }
};

//===----------------------------------------------------------------------===//
// ValueDecl
//
// This class is intentionally generic.  It will become a virtual base for a
// more extensive hierarchy of value declarations later on.
class ValueDecl : public Decl {

protected:
    ValueDecl(AstKind kind, IdentifierInfo *name, Location loc, Type *type)
        : Decl(kind, name, loc),
          type(type) { }

public:
    const Type *getType() const { return type; }

    static bool classof(const ValueDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ParamValueDecl;
    }

protected:
    Type *type;
};

//===----------------------------------------------------------------------===//
// ParamValueDecl
//
// Declaration nodes which represent the formal parameters of a function or
// procedure.  These nodes are owned by the function declaration to which they
// are attached.
class ParamValueDecl : public ValueDecl {

public:
    ParamValueDecl(IdentifierInfo *name, Location loc, DomainType *type)
        : ValueDecl(AST_ParamValueDecl, name, loc, type) { }

    DomainType *getType() { return llvm::cast<DomainType>(type); }

    static bool classof(const ParamValueDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ParamValueDecl;
    }
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
