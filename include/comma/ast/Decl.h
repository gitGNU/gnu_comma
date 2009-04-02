//===-- ast/Decl.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECL_HDR_GUARD
#define COMMA_AST_DECL_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/SignatureSet.h"
#include "comma/ast/Type.h"
#include "comma/basic/ParameterModes.h"
#include "llvm/Support/Casting.h"
#include "llvm/ADT/FoldingSet.h"

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

    // Returns true if this decl is anonymous.
    //
    // FIXME:  This method can me removed once named decls are introduced.
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
// ImportDecl
//
// Represents import declarations.
class ImportDecl : public Decl {

public:
    ImportDecl(Type *target, Location loc)
        : Decl(AST_ImportDecl, 0, loc),
          targetType(target) { }

    Type *getImportedType() { return targetType; }
    const Type *getImportedType() const { return targetType; }

    static bool classof(const ImportDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ImportDecl;
    }

private:
    Type *targetType;
};

//===----------------------------------------------------------------------===//
// TypeDecl
//
// Declarations which correspond to types.
class TypeDecl : public Decl {

public:
    virtual const Type *getType() const = 0;

    Type *getType() {
        return const_cast<Type*>(
            const_cast<const TypeDecl*>(this)->getType());
    }

    static bool classof(const TypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesTypeDecl();
    }

protected:
    TypeDecl(AstKind kind, IdentifierInfo *info, Location loc)
        : Decl(kind, info, loc) {
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
class ModelDecl : public TypeDecl, public DeclarativeRegion {

public:
    virtual ~ModelDecl() { }

    virtual const ModelType *getType() const = 0;

    ModelType *getType() {
        return const_cast<ModelType*>(
            const_cast<const ModelDecl*>(this)->getType());
    }

    // Returns true if this model is parameterized.
    bool isParameterized() const {
        return kind == AST_VarietyDecl || kind == AST_FunctorDecl;
    }

    // Returns the DomainType representing the percent node associated with this
    // decl.
    //
    // For signatures, domains and functors, the DomainType representing % is a
    // unique node owned by the underlying model declaration.  For abstract
    // domains and domain instances, the % node returned is that of the defining
    // declaration (a signature for abstract domains, a domain or functor for
    // instances).  In the latter case, getPercent is simply a forwarding
    // function.
    virtual DomainType *getPercent() const = 0;

    // Accessors to the SignatureSet.
    SignatureSet& getSignatureSet() { return sigset; }
    const SignatureSet &getSignatureSet() const { return sigset; }

    // Adds a direct signature to the underlying signature set.
    bool addDirectSignature(SignatureType *signature) {
        return sigset.addDirectSignature(signature);
    }

    // Support isa and dyn_cast.
    static bool classof(const ModelDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelDecl();
    }

protected:
    ModelDecl(AstKind         kind,
              IdentifierInfo *name,
              Location        loc)
        : TypeDecl(kind, name, loc),
          DeclarativeRegion(kind),
          sigset(this) { }

    // The set of signatures which this model satisfies.
    SignatureSet sigset;
};

//===----------------------------------------------------------------------===//
// Sigoid
//
// This is the common base class for "signature like" objects: i.e. signatures
// and varieties.
class Sigoid : public ModelDecl {

public:
    // Creates a named signature.
    Sigoid(AstKind         kind,
           IdentifierInfo *percentId,
           IdentifierInfo *idInfo,
           Location        loc)
        : ModelDecl(kind, idInfo, loc),
          percent(DomainType::getPercent(percentId, this)) { }

    virtual ~Sigoid() { }

    // If this is a SignatureDecl, returns this cast to the refined type,
    // otherwise returns NULL.
    SignatureDecl *getSignature();

    // If this is a VarietyDecl, returns this cast to the refined type,
    // otherwise returns NULL.
    VarietyDecl *getVariety();

    DomainType *getPercent() const { return percent; }

    static bool classof(const Sigoid *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_SignatureDecl || kind == AST_VarietyDecl;
    }

private:
    DomainType *percent;
};

//===----------------------------------------------------------------------===//
// SignatureDecl
//
// This class defines (non-parameterized) signature declarations.
class SignatureDecl : public Sigoid {

public:
    // Creates a named signature.
    SignatureDecl(IdentifierInfo *percentId,
                  IdentifierInfo *name,
                  const Location &loc);

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
    // Creates a VarietyDecl with the given name, location of definition, and
    // list of AbstractDomainTypes which serve as the formal parameters.
    VarietyDecl(IdentifierInfo *percentId,
                IdentifierInfo *name,
                Location        loc,
                DomainType    **formals,
                unsigned        arity);

    // Returns the type node corresponding to this variety applied over the
    // given arguments.
    SignatureType *getCorrespondingType(Type **args, unsigned numArgs);
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
        return (kind == AST_DomainDecl         ||
                kind == AST_FunctorDecl        ||
                kind == AST_AbstractDomainDecl ||
                kind == AST_DomainInstanceDecl);
    }

protected:
    Domoid(AstKind         kind,
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

    // Returns true if a carrier has been associated with this declaration.
    bool hasCarrier() const { return carrier != 0; }

    // Sets the carrier for this declaration.
    void setCarrier(CarrierDecl *carrier) {
        this->carrier = carrier;
    }

    // Returns the carrier declaration, or NULL if a carrier has not yet been
    // defined.
    CarrierDecl *getCarrier() { return carrier; }
    const CarrierDecl *getCarrier() const { return carrier; }

    static bool classof(const AddDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AddDecl;
    }

private:
    // Non-null if a carrier has been associated with this declaration.
    CarrierDecl *carrier;
};

//===----------------------------------------------------------------------===//
// DomainDecl
//
class DomainDecl : public Domoid {

public:
    DomainDecl(IdentifierInfo *percentId,
               IdentifierInfo *name,
               const Location &loc);

    DomainInstanceDecl *getInstance(Location loc) { return instance; }

    const DomainType *getType() const;
    DomainType *getType();

    // Returns the AddDecl which implements this domain.
    const AddDecl *getImplementation() const { return implementation; }

    DomainType *getPercent() const { return percent; }

    // Support for isa and dyn_cast.
    static bool classof(const DomainDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainDecl;
    }

private:
    DomainInstanceDecl *instance;
    AddDecl            *implementation;
    DomainType         *percent;
};

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
class AbstractDomainDecl : public Domoid {

public:
    AbstractDomainDecl(IdentifierInfo *name,
                       SignatureType  *type,
                       Location        loc);

    const DomainType *getType() const { return abstractType; }
    DomainType *getType() { return abstractType; }

    SignatureType *getSignatureType() const { return signature; }

    DomainType *getPercent() const {
        return signature->getDeclaration()->getPercent();
    }

    static bool classof(const AbstractDomainDecl *node) { return true; }
    static bool classof(const Ast* node) {
        return node->getKind() == AST_AbstractDomainDecl;
    }

private:
    DomainType    *abstractType;
    SignatureType *signature;
};

//===----------------------------------------------------------------------===//
// DomainInstanceDecl
class DomainInstanceDecl : public Domoid, public llvm::FoldingSetNode {

public:
    DomainInstanceDecl(DomainDecl *domain, Location loc);

    DomainInstanceDecl(FunctorDecl *functor,
                       Type       **args,
                       unsigned     numArgs,
                       Location     loc);

    const DomainType *getType() const { return correspondingType; }
    DomainType *getType() { return correspondingType; }

    Domoid *getDefiningDecl() const { return definition; }

    DomainDecl  *getDefiningDomain() const;
    FunctorDecl *getDefiningFunctor() const;

    DomainType *getPercent() const {
        return getDefiningDecl()->getPercent();
    }

    // Returns the arity of the underlying declaration.
    unsigned getArity() const;

    // Returns the i'th actual parameter.  This function asserts if its argument
    // is out of range,
    Type *getActualParameter(unsigned n) const {
        assert(n < getArity() && "Index out of range!");
        return arguments[n];
    }

    // Returns true if this domain type is an instance of some functor.
    bool isParameterized() const { return getArity() != 0; }

    typedef Type **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    // Called by FunctorDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id, Type **args, unsigned numArgs);

    static bool classof(const DomainInstanceDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainInstanceDecl;
    }

private:
    Domoid     *definition;
    Type      **arguments;
    DomainType *correspondingType;

    // The following call-backs are invoked when the declarative region of the
    // defining declaration changes.
    void notifyAddDecl(Decl *decl);
    void notifyRemoveDecl(Decl *decl);
};

//===----------------------------------------------------------------------===//
// FunctorDecl
//
// Representation of parameterized domains.
class FunctorDecl : public Domoid {

public:
    FunctorDecl(IdentifierInfo *percentId,
                IdentifierInfo *name,
                Location        loc,
                DomainType    **formals,
                unsigned        arity);

    // Returns an instance declaration corresponding to this functor applied
    // over the given set of arguments.  Such instance declarations are
    // memoized, and for a given set of arguments this method always returns the
    // same declaration node.  As a consequence, the location associated with
    // any given DomainInstanceDecl corresponds to the first location the
    // instance was processed.
    DomainInstanceDecl *getInstance(Type   **args,
                                    unsigned numArgs,
                                    Location loc = 0);

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

    DomainType *getPercent() const { return percent; }

    // Support for isa and dyn_cast.
    static bool classof(const FunctorDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorDecl;
    }

private:
    mutable llvm::FoldingSet<DomainInstanceDecl> instances;

    FunctorType *functor;
    AddDecl     *implementation;
    DomainType  *percent;
};

//===----------------------------------------------------------------------===//
// CarrierDecl
//
// Declaration of a domains carrier type.
class CarrierDecl : public TypeDecl {

public:
    CarrierDecl(IdentifierInfo *name, Type *type, Location loc)
        : TypeDecl(AST_CarrierDecl, name, loc),
          carrierType(new CarrierType(this)),
          representation(type) { }

    const CarrierType *getType() const { return carrierType; }
    CarrierType *getType() { return carrierType; }

    const Type *getRepresentationType() const { return representation; }
    Type *getRepresentationType() { return representation; }

    static bool classof(const CarrierDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_CarrierDecl;
    }

private:
    CarrierType *carrierType;
    Type        *representation;
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
                   Type              *returnType,
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

    int getKeywordIndex(IdentifierInfo *key) const {
        return routineType->getKeywordIndex(key);
    }

    Type *getArgType(unsigned i) const {
        return routineType->getArgType(i);
    }

    typedef ParamValueDecl **ParamDeclIterator;

    ParamDeclIterator beginParams() { return parameters; }
    ParamDeclIterator endParams()   { return parameters + getArity(); }

    void setBaseDeclaration(SubroutineDecl *routineDecl);
    SubroutineDecl *getBaseDeclaration() { return baseDeclaration; }
    const SubroutineDecl *getBaseDeclaration() const { return baseDeclaration; }

    bool hasBody() const;
    void setBody(BlockStmt *block) { body = block; }

    BlockStmt *getBody();

    const BlockStmt *getBody() const {
        return const_cast<SubroutineDecl*>(this)->getBody();
    }

    void dump();

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
                 Type              *returnType,
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

    Type *getReturnType() const {
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

    ProcedureDecl(IdentifierInfo    *name,
                  Location           loc,
                  ProcedureType     *type,
                  DeclarativeRegion *parent)
        : SubroutineDecl(AST_ProcedureDecl,
                         name, loc,
                         type, parent) { }

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
    ValueDecl(AstKind kind, IdentifierInfo *name, Type *type, Location loc)
        : Decl(kind, name, loc),
          type(type) {
        assert(this->denotesValueDecl());
    }

public:
    const Type *getType() const { return type; }
    Type       *getType()       { return type; }

    static bool classof(const ValueDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesValueDecl();
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
    ParamValueDecl(IdentifierInfo *name,
                   Type           *type,
                   ParameterMode   mode,
                   Location        loc)
        : ValueDecl(AST_ParamValueDecl, name, type, loc) {
        // Store the mode for this decl in the bit field provided by our
        // base Ast instance.
        //
        // FIXME: This is bad practice, really.  But the bits are available so
        // we use them.  Eventually, a better interface/convention should be
        // established to help protect against the bit field being trashed, or
        // this data should be moved into the class itself.
        bits = mode;
    }

    /// Returns true if the parameter mode was explicitly specified for this
    /// parameter.  This predicate is used to distinguish between the default
    /// parameter mode of "in" and the case where "in" was explicitly given.
    bool parameterModeSpecified() const;

    /// Returns the parameter mode associated with this decl.  This function
    /// never returns MODE_DEFAULT, only MODE_IN.  To check if the mode was
    /// implicitly defined as "in" use parameterModeSpecified, or call
    /// getExplicitParameterMode.
    ParameterMode getParameterMode() const;

    /// \brief Returns the parameter mdoe associated with this decl.
    ParameterMode getExplicitParameterMode() const;

    static bool classof(const ParamValueDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ParamValueDecl;
    }
};

//===----------------------------------------------------------------------===//
// ObjectDecl
//
// Object declarations denote objects of a given type.  They may optionally be
// associated with an initial value given by an expression.
class ObjectDecl : public ValueDecl {

public:
    ObjectDecl(IdentifierInfo *name,
               Type           *type,
               Location        loc,
               Expr           *init = 0)
        : ValueDecl(AST_ObjectDecl, name, type, loc),
          initialization(init) { }

    // Returns true if this object declaration is associated with an
    // initialization expression.
    bool hasInitializer() const { return initialization != 0; }

    // Returns the initialization expression associated with this object decl,
    // or NULL if there is no such association.
    Expr *getInitializer() const { return initialization; }

    // Sets the initialization expression for this declaration.  Owership of the
    // expression is passed to the declaration.
    void setInitializer(Expr *init) { initialization = init; }

    // Support isa and dyn_cast.
    static bool classof(const ObjectDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ObjectDecl;
    }

private:
    Expr *initialization;
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

inline const DomainType *DomainDecl::getType() const {
    return instance->getType();
}

inline DomainType *DomainDecl::getType() {
    return instance->getType();
}

inline DomainDecl *DomainInstanceDecl::getDefiningDomain() const
{
    return llvm::dyn_cast<DomainDecl>(definition);
}

inline FunctorDecl *DomainInstanceDecl::getDefiningFunctor() const
{
    return llvm::dyn_cast<FunctorDecl>(definition);
}

} // End comma namespace

#endif
