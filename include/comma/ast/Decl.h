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
#include "comma/ast/DeclRegion.h"
#include "comma/ast/SignatureSet.h"
#include "comma/ast/Type.h"
#include "comma/basic/ParameterModes.h"
#include "comma/basic/PrimitiveOps.h"
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

    // Sets the declarative region for this decl.  This function can only be
    // called once to initialize the decl.
    void setDeclRegion(DeclRegion *region) {
        assert(context == 0 && "Cannot reset a decl's declarative region!");
        context = region;
    }

    // Returns the declarative region for this decl.  Sometimes decls are
    // created before their associated regions exist, so this method may return
    // null.
    DeclRegion *getDeclRegion() { return context; }
    const DeclRegion *getDeclRegion() const { return context; }

    // Returns true if this decl was declared in the given region.
    bool isDeclaredIn(const DeclRegion *region) const {
        return region == context;
    }

    /// Returns this cast to a DeclRegion, or null if this decl is not also a
    /// declarative region.
    DeclRegion *asDeclRegion();

    // Support isa and dyn_cast.
    static bool classof(const Decl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDecl();
    }

protected:
    Decl(AstKind kind, IdentifierInfo *info = 0, Location loc = 0,
         DeclRegion *region = 0)
        : Ast(kind),
          idInfo(info),
          location(loc),
          context(region) {
        assert(this->denotesDecl());
        deletable = false;
    }

    IdentifierInfo *idInfo;
    Location location;
    DeclRegion *context;
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
// ModelDecl
//
// Models represent those attributes and characteristics which both signatures
// and domains share.
class ModelDecl : public Decl {

public:
    virtual ~ModelDecl();

    /// Returns true if this model is parameterized.
    bool isParameterized() const {
        return kind == AST_VarietyDecl || kind == AST_FunctorDecl;
    }

    /// Returns the number of arguments accepted by this model.
    virtual unsigned getArity() const;

    /// Returns the abstract domain declaration corresponding the i'th formal
    /// parameter.  This method will assert if this declaration is not
    /// parameterized.
    virtual AbstractDomainDecl *getFormalDecl(unsigned i) const;

    /// Returns the index of the given AbstractDomainDecl which must be a formal
    /// parameter of this model.  This method will assert if this declaration is not
    /// parameterized.
    unsigned getFormalIndex(const AbstractDomainDecl *ADDecl) const;

    /// Returns the type of the i'th formal formal parameter.  This method will
    /// assert if this declaration is not parameterized.
    DomainType *getFormalType(unsigned i) const;

    /// Returns the SigInstanceDecl which the i'th actual parameter must
    /// satisfy.  This method will assert if this declaration is not
    /// parameterized.
    SigInstanceDecl *getFormalSignature(unsigned i) const;

    /// Returns the IdentifierInfo which labels the i'th formal parameter.  This
    /// method will assert if this declaration is not parameterized.
    IdentifierInfo *getFormalIdInfo(unsigned i) const;

    /// Returns the index of the parameter corresponding to the given keyword,
    /// or -1 if no such keyword exists.  This method will assert if this
    /// declaration is not parameterized.
    int getKeywordIndex(IdentifierInfo *keyword) const;

    /// Returns the index of the parameter corresponding to the given keyword
    /// selector or -1 if no such keyword exists.  This method will assert if
    /// this declaration is not parameterized.
    int getKeywordIndex(KeywordSelector *selector) const;

    /// Returns the PercentDecl representing this Model.
    PercentDecl *getPercent() const { return percent; }

    /// Returns the DomainType representing percent.
    DomainType *getPercentType() const;

    /// Returns the signature set of the assoiated percent node.
    const SignatureSet &getSignatureSet() const;

    /// Adds a direct signature to the signature set or this models percent
    /// node.
    bool addDirectSignature(SigInstanceDecl *signature);

    /// Returns the AstResource object associated with this model.
    ///
    /// This method is intended for use by other nodes in the AST, not by
    /// clients of the AST itself.
    AstResource &getAstResource() { return resource; }

    // Support isa and dyn_cast.
    static bool classof(const ModelDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesModelDecl();
    }

protected:
    ModelDecl(AstResource &resource,
              AstKind kind, IdentifierInfo *name, Location loc);

    // The unique PercentDecl representing this model.
    PercentDecl *percent;

    // The AstResource we use to construct sub-nodes.
    AstResource &resource;
};

//===----------------------------------------------------------------------===//
// Sigoid
//
// This is the common base class for "signature like" objects: i.e. signatures
// and varieties.
class Sigoid : public ModelDecl {

public:
    // Creates a named signature.
    Sigoid(AstResource &resource,
           AstKind kind, IdentifierInfo *name, Location loc)
        : ModelDecl(resource, kind, name, loc) { }

    virtual ~Sigoid() { }

    // If this is a SignatureDecl, returns this cast to the refined type,
    // otherwise returns NULL.
    SignatureDecl *getSignature();

    // If this is a VarietyDecl, returns this cast to the refined type,
    // otherwise returns NULL.
    VarietyDecl *getVariety();

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
    // Creates a named signature.
    SignatureDecl(AstResource &resource,
                  IdentifierInfo *name, const Location &loc);

    SigInstanceDecl *getInstance() { return theInstance; }

    // Support for isa and dyn_cast.
    static bool classof(const SignatureDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SignatureDecl;
    }

private:
    // The unique instance decl representing this signature.
    SigInstanceDecl *theInstance;
};

//===----------------------------------------------------------------------===//
// VarietyDecl
//
// Repesentation of parameterized signatures.
class VarietyDecl : public Sigoid {

public:
    // Creates a VarietyDecl with the given name, location of definition, and
    // list of AbstractDomainTypes which serve as the formal parameters.
    VarietyDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                AbstractDomainDecl **formals, unsigned arity);

    /// Returns the instance decl corresponding to this variety applied over the
    /// given arguments.
    SigInstanceDecl *getInstance(DomainTypeDecl **args, unsigned numArgs);

    /// Returns the number of arguments accepted by this variety.
    unsigned getArity() const { return arity; }

    /// Returns the abstract domain representing the i'th formal parameter.
    AbstractDomainDecl *getFormalDecl(unsigned i) const {
        assert(i < arity && "Index out of range!");
        return formalDecls[i];
    }

    /// Iterator over the all of the signature instances which represent
    /// specific parameterizations of this variety.
    typedef llvm::FoldingSet<SigInstanceDecl>::iterator instance_iterator;
    instance_iterator begin_instances() { return instances.begin(); }
    instance_iterator end_instances() { return instances.end(); }

    // Support for isa and dyn_cast.
    static bool classof(const VarietyDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_VarietyDecl;
    }

private:
    /// A FoldingSet of all signature instances representing specific
    /// parameterizations of this variety.
    mutable llvm::FoldingSet<SigInstanceDecl>  instances;

    unsigned arity;                   ///< The number of formal parameters.
    AbstractDomainDecl **formalDecls; ///< The formal parameter declarations.
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
        return (kind == AST_DomainDecl or kind == AST_FunctorDecl);
    }

protected:
    Domoid(AstResource &resource,
           AstKind kind, IdentifierInfo *idInfo, Location loc);
};

//===----------------------------------------------------------------------===//
// AddDecl
//
// This class represents an add expression.  It provides a declarative region
// for the body of a domain and contains all function and values which the
// domain defines.
class AddDecl : public Decl, public DeclRegion {

public:
    // Creates an AddDecl to represent the body of the given domain.
    AddDecl(DomainDecl *domain);

    // Creates an AddDecl to represent the body of the given functor.
    AddDecl(FunctorDecl *functor);

    // Returns true if this Add implements a DomainDecl.
    bool implementsDomain() const;

    // Returns true if this Add implements a FunctorDecl.
    bool implementsFunctor() const;

    // Returns the domoid which this add implements.
    Domoid *getImplementedDomoid();

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
    DomainDecl(AstResource &resource,
               IdentifierInfo *name, const Location &loc);

    DomainInstanceDecl *getInstance();

    // Returns the AddDecl which implements this domain.
    const AddDecl *getImplementation() const { return implementation; }

    // Support for isa and dyn_cast.
    static bool classof(const DomainDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainDecl;
    }

private:
    DomainInstanceDecl *instance;
    AddDecl *implementation;
};

//===----------------------------------------------------------------------===//
// FunctorDecl
//
// Representation of parameterized domains.
class FunctorDecl : public Domoid {

public:
    FunctorDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                AbstractDomainDecl **formals, unsigned arity);

    // Returns an instance declaration corresponding to this functor applied
    // over the given set of arguments.  Such instance declarations are
    // memoized, and for a given set of arguments this method always returns the
    // same declaration node.
    DomainInstanceDecl *getInstance(DomainTypeDecl **args, unsigned numArgs);

    // Returns the AddDecl which implements this functor.
    const AddDecl *getImplementation() const { return implementation; }

    /// Returns the number of arguments accepted by this functor.
    unsigned getArity() const { return arity; }

    /// Returns the abstract domain representing the i'th formal parameter.
    AbstractDomainDecl *getFormalDecl(unsigned i) const {
        assert(i < arity && "Index out of range!");
        return formalDecls[i];
    }

    // Support for isa and dyn_cast.
    static bool classof(const FunctorDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FunctorDecl;
    }

private:
    /// Set of all DomainInstanceDecl's which represent instances of this
    /// functor.
    mutable llvm::FoldingSet<DomainInstanceDecl> instances;

    unsigned arity;                   ///< Number of formal parameters.x
    AbstractDomainDecl **formalDecls; ///< Formal parameter declarations.
    AddDecl *implementation;          ///< Body of this functor.
};

//===----------------------------------------------------------------------===//
// SigInstanceDecl

class SigInstanceDecl : public Decl, public llvm::FoldingSetNode {

public:
    Sigoid *getSigoid() { return underlyingSigoid; }
    const Sigoid *getSigoid() const { return underlyingSigoid; }

    SignatureDecl *getSignature() const;

    VarietyDecl *getVariety() const;

    /// Returns true if this type represents an instance of some variety.
    bool isParameterized() const { return getVariety() != 0; }

    /// Returns the number of actual arguments supplied.  When the underlying
    /// model is a signature, the arity is zero.
    unsigned getArity() const;

    /// Returns the i'th actual parameter.  This method asserts if its argument
    /// is out of range.
    DomainTypeDecl *getActualParam(unsigned n) const {
        assert(isParameterized() &&
               "Cannot fetch parameter from non-parameterized type!");
        assert(n < getArity() && "Parameter index out of range!");
        return arguments[n];
    }

    /// Returns the type of the i'th actual parameter.
    DomainType *getActualParamType(unsigned n) const;

    typedef DomainTypeDecl **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    /// For use by llvm::FoldingSet.
    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    static bool classof(const SigInstanceDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_SigInstanceDecl;
    }

private:
    friend class SignatureDecl;
    friend class VarietyDecl;

    SigInstanceDecl(SignatureDecl *decl);

    SigInstanceDecl(VarietyDecl *decl, DomainTypeDecl **args, unsigned numArgs);

    // Called by VarietyDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id,
            DomainTypeDecl **args, unsigned numArgs);

    // The Sigoid supporing this type.
    Sigoid *underlyingSigoid;

    // If the supporting declaration is a variety, then this array contains the
    // actual arguments defining this instance.
    DomainTypeDecl **arguments;
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
          correspondingType(type) {
        assert(this->denotesValueDecl());
    }

public:
    const Type *getType() const { return correspondingType; }
    Type *getType() { return correspondingType; }

    static bool classof(const ValueDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesValueDecl();
    }

protected:
    Type *correspondingType;
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
                   Type *type,
                   PM::ParameterMode mode,
                   Location loc)
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
    PM::ParameterMode getParameterMode() const;

    /// \brief Returns the parameter mdoe associated with this decl.
    PM::ParameterMode getExplicitParameterMode() const;

    /// \brief Sets the parameter mode of this decl.
    void setParameterMode(PM::ParameterMode mode);

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
// SubroutineDecl
//
// Base class for representing procedures and functions.
class SubroutineDecl : public Decl, public DeclRegion {

public:
    virtual ~SubroutineDecl();

    /// Returns the type of this declaration.
    virtual SubroutineType *getType() const = 0;

    /// Returns the number of parameters this subroutine accepts.
    unsigned getArity() const { return numParameters; }

    /// Returns the i'th parameters declaration node.
    ParamValueDecl *getParam(unsigned i) {
        assert(i < getArity() && "Index out of range!");
        return parameters[i];
    }

    /// Returns the i'th parameters declaration node.
    const ParamValueDecl *getParam(unsigned i) const {
        assert(i < getArity() && "Index out of range!");
        return parameters[i];
    }

    /// Returns the type of the i'th parameter.
    Type *getParamType(unsigned i) const {
        return getType()->getArgType(i);
    }

    /// Returns the i'th parameter mode.
    ///
    /// Parameters with MODE_DEFAULT are automatically converted to MODE_IN (if
    /// this conversion is undesirable use getExplicitParameterMode instead).
    PM::ParameterMode getParamMode(unsigned i) const {
        return getParam(i)->getParameterMode();
    }

    /// Returns the i'th parameter mode for this type.
    PM::ParameterMode getExplicitParamMode(unsigned i) const {
        return getParam(i)->getExplicitParameterMode();
    }

    /// Returns the i'th argument keyword.
    IdentifierInfo *getParamKeyword(unsigned i) const {
        return getParam(i)->getIdInfo();
    }

    /// If \p key names an argument keyword, return its associated index, else
    /// return -1.
    int getKeywordIndex(IdentifierInfo *key) const;

    /// If the KeywordSelector \p key names an argument keyword, return its
    /// associated index, else return -1.
    int getKeywordIndex(KeywordSelector *key) const;

    /// Returns true if the keywords of the declaration match exactly those of
    /// this one.  The arity of both subroutines must match for this function to
    /// return true.
    bool keywordsMatch(const SubroutineDecl *SRDecl) const;

    /// Returns true if the parameter modes of the given declaration match those
    /// of this one.  The arity of both subroutines must match of this function
    /// to return true.
    bool paramModesMatch(const SubroutineDecl *SRDecl) const;

    /// \name Parameter Iterators
    ///
    ///@{
    typedef ParamValueDecl **param_iterator;
    param_iterator begin_params() { return parameters; }
    param_iterator end_params() { return parameters + getArity(); }

    typedef ParamValueDecl *const *const_param_iterator;
    const_param_iterator begin_params() const { return parameters; }
    const_param_iterator end_params() const { return parameters + getArity(); }
    ///@}

    void setDefiningDeclaration(SubroutineDecl *routineDecl);
    SubroutineDecl *getDefiningDeclaration() { return definingDeclaration; }
    const SubroutineDecl *getDefiningDeclaration() const {
        return definingDeclaration;
    }

    bool hasBody() const;
    void setBody(BlockStmt *block) { body = block; }
    BlockStmt *getBody();
    const BlockStmt *getBody() const {
        return const_cast<SubroutineDecl*>(this)->getBody();
    }

    /// Returns true if this declaration is immediate.
    ///
    /// An immediate declaration is one which directly corresponds to a
    /// declaration voiced in the source code -- as opposed to one implicitly
    /// generated by the compiler.  The canonical example of a non-immediate
    /// declaration is one which was inherited from a supersignature.
    bool isImmediate() const { return immediate; }

    /// Mark this declaration as immediate.
    void setImmediate() { immediate = true; }

    /// Returns the origin of this decl, or null if there is no associated
    /// origin.
    ///
    /// A declaration has an origin if it is not an immediate declaration.  That
    /// is to say, the declaration was implicitly generated due to inheritance
    /// from a supersignature.  The returned node is the actual declaration
    /// object provided by some supersignature.
    ///
    /// \see isImmediate
    SubroutineDecl *getOrigin() { return origin; }
    const SubroutineDecl *getOrigin() const { return origin; }

    /// Returns true if this decl has an origin.
    bool hasOrigin() const { return origin != 0; }

    /// Sets the origin of this decl.
    void setOrigin(SubroutineDecl *decl) { origin = decl; }

    /// Walks the chain of origins returning the final non-null declaration;
    SubroutineDecl *resolveOrigin();
    const SubroutineDecl *resolveOrigin() const {
        return const_cast<SubroutineDecl*>(this)->resolveOrigin();
    }

    /// Returns true if this subroutine represents a primitive operation.
    bool isPrimitive() const { return opID != PO::NotPrimitive; }

    /// Marks this declaration as primitive.
    void setAsPrimitive(PO::PrimitiveID ID) { opID = ID; }

    /// Returns the PrimitiveID of this subroutine.
    PO::PrimitiveID getPrimitiveID() const { return opID; }

    /// Returns true if this subroutine is an overriding declaration.
    bool isOverriding() const { return overriddenDecl != 0; }

    /// Returns the declaration this one overrides, or null if this is not an
    /// overriding declaration.
    const SubroutineDecl *getOverriddenDecl() const { return overriddenDecl; }

    /// Sets this declaration as overriding the given subroutine.
    void setOverriddenDecl(SubroutineDecl *decl);

    // Support for isa and dyn_cast.
    static bool classof(const SubroutineDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesSubroutineDecl();
    }

protected:
    // Subroutine decls take ownership of any ParamValueDecls supplied (but not
    // the array they are passed in).
    SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                   ParamValueDecl **params, unsigned numParams,
                   DeclRegion *parent);

    SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                   IdentifierInfo **keywords, SubroutineType *type,
                   DeclRegion *parent);

    bool immediate       : 1;   ///< Set if the declaration is immediate.
    PO::PrimitiveID opID : 7;   ///< Identifies the type of operation.

    unsigned numParameters;
    ParamValueDecl **parameters;
    BlockStmt *body;
    SubroutineDecl *definingDeclaration;
    SubroutineDecl *origin;
    SubroutineDecl *overriddenDecl;
};

//===----------------------------------------------------------------------===//
// ProcedureDecl
//
// Representation of procedure declarations.
class ProcedureDecl : public SubroutineDecl {

public:
    ProcedureDecl(AstResource &resource,
                  IdentifierInfo *name, Location loc,
                  ParamValueDecl **params, unsigned numParams,
                  DeclRegion *parent);

    /// Constructs a Procedure given a ProcedureType and set of keywords.
    ///
    /// This constructor is most useful for generating implicit declarations,
    /// typically using a rewritten type.  ParamValue decls are generated using
    /// the supplied array of keywords (which must be long enough to match the
    /// arity of the supplied type, or 0 if this is a nullary procedure).  The
    /// resulting parameter decls all have default modes, and so one must set
    /// each by hand if need be afterwords.
    ProcedureDecl(IdentifierInfo *name, Location loc,
                  IdentifierInfo **keywords, ProcedureType *type,
                  DeclRegion *parent)
        : SubroutineDecl(AST_ProcedureDecl, name, loc, keywords, type, parent),
          correspondingType(type) { }

    ProcedureDecl(IdentifierInfo *name, Location loc,
                  ProcedureType *type, DeclRegion *parent);

    ProcedureType *getType() const { return correspondingType; }

    ProcedureDecl *getDefiningDeclaration() {
        return llvm::cast_or_null<ProcedureDecl>(definingDeclaration);
    }

    const ProcedureDecl *getDefiningDeclaration() const {
        return const_cast<ProcedureDecl*>(this)->getDefiningDeclaration();
    }

    /// Returns the declaration this one overrides, or null if this is not an
    /// overriding declaration.
    const ProcedureDecl *getOverriddenDecl() const {
        return llvm::dyn_cast_or_null<ProcedureDecl>(overriddenDecl);
    }

    // Support for isa and dyn_cast.
    static bool classof(const ProcedureDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ProcedureDecl;
    }

private:
    ProcedureType *correspondingType;
};

//===----------------------------------------------------------------------===//
// FunctionDecl
//
// Representation of function declarations.
class FunctionDecl : public SubroutineDecl {

public:
    FunctionDecl(AstResource &resource,
                 IdentifierInfo *name, Location loc,
                 ParamValueDecl **params, unsigned numParams,
                 Type *returnType, DeclRegion *parent);

    /// Constructs a FunctionDecl given a FunctionType and set of keywords.
    ///
    /// This constructor is most useful for generating implicit declarations,
    /// typically using a rewritten type.  ParamValue decls are generated using
    /// the supplied array of keywords (which must be long enough to match the
    /// arity of the supplied type, or 0 if this is a nullary function).  The
    /// resulting parameter decls all have default modes, and so one must set
    /// each by hand if need be afterwords.
    FunctionDecl(IdentifierInfo *name, Location loc,
                 IdentifierInfo **keywords, FunctionType *type,
                 DeclRegion *parent)
        : SubroutineDecl(AST_FunctionDecl, name, loc, keywords, type, parent),
          correspondingType(type) { }

    FunctionType *getType() const { return correspondingType; }

    FunctionDecl *getDefiningDeclaration() {
        return llvm::cast_or_null<FunctionDecl>(definingDeclaration);
    }

    const FunctionDecl *getDefiningDeclaration() const {
        return const_cast<FunctionDecl*>(this)->getDefiningDeclaration();
    }

    /// Returns the declaration this one overrides, or null if this is not an
    /// overriding declaration.
    const FunctionDecl *getOverriddenDecl() const {
        return llvm::dyn_cast_or_null<FunctionDecl>(overriddenDecl);
    }

    Type *getReturnType() const { return getType()->getReturnType(); }

    // Support for isa and dyn_cast.
    static bool classof(const FunctionDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return (node->getKind() == AST_FunctionDecl ||
                node->getKind() == AST_EnumLiteral);
    }

protected:
    // Constructor used by derived function-like declarations (EnumLiteral, in
    // particular).
    FunctionDecl(AstKind kind, AstResource &resource,
                 IdentifierInfo *name, Location loc,
                 ParamValueDecl **params, unsigned numParams,
                 Type *returnType, DeclRegion *parent);

private:
    FunctionType *correspondingType;

    void initializeCorrespondingType(AstResource &resource, Type *returnType);
};

//===----------------------------------------------------------------------===//
// EnumLiteral
//
// Instances of this class represent the elements of an EnumerationDecl.
class EnumLiteral : public FunctionDecl {

public:
    /// Returns the index (or value) of this EnumLiteral.
    unsigned getIndex() const { return index; }

    static bool classof(const EnumLiteral *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumLiteral;
    }

private:
    // Enumeration literals are constructed by their containing enumeration decl
    // node.
    EnumLiteral(AstResource &resource, IdentifierInfo *name, Location loc,
                unsigned index, EnumerationDecl *parent);

    friend class EnumerationDecl;

    unsigned index;
};

//===----------------------------------------------------------------------===//
// TypeDecl
//
// All nodes which declare types inherit from this class.
class TypeDecl : public Decl {

public:

    // Returns the type of this TypeDecl.
    Type *getType() const { return CorrespondingType; }

    static bool classof(const TypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesTypeDecl();
    }

protected:
    // Constructs a TypeDecl node when a type is immediately available.
    TypeDecl(AstKind kind, IdentifierInfo *name, Type *type, Location loc)
        : Decl(kind, name, loc),
          CorrespondingType(type) {
        assert(this->denotesTypeDecl());
    }

    // Constructs a TypeDecl node when a type is not immediately available.
    // Users of this constructor must set the corresponding type.
    TypeDecl(AstKind kind, IdentifierInfo *name, Location loc)
        : Decl(kind, name, loc),
          CorrespondingType(0) {
        assert(this->denotesTypeDecl());
    }

    Type *CorrespondingType;
};

//===----------------------------------------------------------------------===//
// CarrierDecl
//
// Declaration of a domains carrier type.
//
// FIXME: A CarrierDecl should not be a TypeDecl, but rather a SubTypeDecl.
class CarrierDecl : public TypeDecl {

public:
    CarrierDecl(IdentifierInfo *name, Type *type, Location loc)
        : TypeDecl(AST_CarrierDecl, name, loc) {
        CorrespondingType = new CarrierType(this, type);
    }

    CarrierType *getType() const {
        return llvm::cast<CarrierType>(CorrespondingType);
    }

    const Type *getRepresentationType() const {
        return getType()->getParentType();
    }

    Type *getRepresentationType() {
        return getType()->getParentType();
    }

    static bool classof(const CarrierDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_CarrierDecl;
    }
};

//===----------------------------------------------------------------------===//
// EnumerationDecl
class EnumerationDecl : public TypeDecl, public DeclRegion {

public:
    /// Populates the declarative region of this type with all implicit
    /// operations.  This must be called once the type has been constructed to
    /// gain access to the types operations.
    void generateImplicitDeclarations(AstResource &resource);

    EnumSubType *getType() const {
        return llvm::cast<EnumSubType>(CorrespondingType);
    }

    // Returns the number of EnumLiteral's associated with this enumeration.
    unsigned getNumLiterals() const { return numLiterals; }

    // Returns the literal with the given name, or null if no such literal is a
    // member of this enumeration.
    EnumLiteral *findLiteral(IdentifierInfo *name);

    // Marks this declaration as a character enumeration.
    //
    // This method should be called if any of the literals constituting this
    // declaration are character literals.
    void markAsCharacterType() { bits = 1; }

    // Returns true if this declaration denotes a character enumeration.
    bool isCharacterType() const { return bits == 1; }

    static bool classof(const EnumerationDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumerationDecl;
    }

private:
    // Private constructors for use by AstResource.
    EnumerationDecl(AstResource &resource,
                    IdentifierInfo *name, Location loc,
                    std::pair<IdentifierInfo*, Location> *elems,
                    unsigned numElems, DeclRegion *parent);

    friend class AstResource;

    // The number of EnumLiteral's associated with this enumeration.
    uint32_t numLiterals;
};

//===----------------------------------------------------------------------===//
// IntegerDecl
//
// These nodes represent integer type declarations.
class IntegerDecl : public TypeDecl, public DeclRegion {

public:
    /// Populates the declarative region of this type with all implicit
    /// operations.  This must be called once the type has been constructed to
    /// gain access to the types operations.
    void generateImplicitDeclarations(AstResource &resource);

    /// Returns the prefered subtype of this integer declaration.
    ///
    /// This method returns the first subtype of this declaration, or in the
    /// special case of root_integer, the unconstrained base subtype.
    IntegerSubType *getType() const {
        return llvm::cast<IntegerSubType>(CorrespondingType);
    }

    /// Returns the base subtype of this integer type declaration.
    IntegerSubType *getBaseSubType() const {
        return getType()->getTypeOf()->getBaseSubType();
    }

    /// Returns the expression forming the lower bound of this integer
    /// declaration.
    Expr *getLowBoundExpr() { return lowExpr; }
    const Expr *getLowBoundExpr() const { return lowExpr; }

    /// Returns the expression forming the upper bound of this integer
    /// declaration.
    Expr *getHighBoundExpr() { return highExpr; }
    const Expr *getHighBoundExpr() const { return highExpr; }

    static bool classof(const IntegerDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerDecl;
    }

private:
    /// Private constructor for use by AstResource.
    IntegerDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                Expr *lowRange, Expr *highRange,
                const llvm::APInt &lowVal, const llvm::APInt &highVal,
                DeclRegion *parent);

    friend class AstResource;

    Expr *lowExpr;              ///< Expr forming the lower bound.
    Expr *highExpr;             ///< Expr forming the high bound.
};

//===----------------------------------------------------------------------===//
// ArrayDecl
//
// This node represents array type declarations.
class ArrayDecl : public TypeDecl, public DeclRegion {

public:
    ArraySubType *getType() const {
        return llvm::cast<ArraySubType>(CorrespondingType);
    }

    /// Returns the rank of this array declaration.
    unsigned getRank() const { return getType()->getRank(); }

    /// Returns the type describing the i'th index of this array.
    SubType *getIndexType(unsigned i) const {
        return getType()->getIndexType(i);
    }

    /// Returns the type describing the component type of this array.
    Type *getComponentType() const {
        return getType()->getComponentType();
    }

    /// Returns true if this declaration is constrained.
    bool isConstrained() const { return getType()->isConstrained(); }

    //@{
    /// Iterators over the index types.
    typedef SubType **index_iterator;
    index_iterator begin_indices() { return getType()->begin_indices(); }
    index_iterator end_indices() { return getType()->end_indices(); }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const ArrayDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ArrayDecl;
    }

private:
    /// Private constructor for use by AstResource.
    ArrayDecl(AstResource &resource,
              IdentifierInfo *name, Location loc,
              unsigned rank, SubType **indices,
              Type *component, bool isConstrained, DeclRegion *parent);

    friend class AstResource;
};

//===----------------------------------------------------------------------===//
// DomainTypeDecl
//
// This class represents implicit domain declarations which correspond to a
// particular instance.  There are three main subclasses:
//
//   - DomainInstanceDecl's represent the public or external view of a domain.
//     All references to formal parameters are replaced by the actuals for a
//     particular instance, and all percent nodes are mapped to the type of this
//     instance.
//
//   - AbstractDomainDecl's represent the formal parameters of a model.  They
//     provide a view of a domain as restricted by their principle signature.
//     These types of declarations are only visible in the bodies of generic
//     models.  Such domains provide a rewritten interface to the principle
//     signature where the signatures percent node is replaced by references to
//     this type, and where the formal arguments of a variety are replaced by
//     the actuals.
//
//   - PercentDecl's are the nodes which encapsulate the internal view of a
//     model.
//
class DomainTypeDecl : public TypeDecl, public DeclRegion {

protected:
    DomainTypeDecl(AstKind kind, IdentifierInfo *name, Location loc = 0);

public:
    virtual ~DomainTypeDecl() { }

    /// Returns the SignatureSet of this DomainTypeDecl.
    ///
    /// The signatures of a DomainTypeDecl are a rewritten version of those
    /// provided by the defining domoid.  In particular, references to % are
    /// replaced by references to this declarations type, and formal parameters
    /// (when the definition is a functor) are replaced by the actual
    /// parameters.
    virtual const SignatureSet &getSignatureSet() const = 0;

    DomainType *getType() const {
        return llvm::cast<DomainType>(CorrespondingType);
    }

    static bool classof(const DomainTypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDomainTypeDecl();
    }
};

//===----------------------------------------------------------------------===//
// AbstractDomainDecl
class AbstractDomainDecl : public DomainTypeDecl {

public:
    AbstractDomainDecl(IdentifierInfo *name, Location loc)
        : DomainTypeDecl(AST_AbstractDomainDecl, name, loc) { }

    /// Returns the SignatureSet of this abstract domain.
    const SignatureSet &getSignatureSet() const { return sigset; }

    /// Adds a direct super signature to this decl.
    ///
    /// Returns true if the given signature was added to the set, or false if it
    /// was already registered with this declaration.
    bool addSuperSignature(SigInstanceDecl *sig);

    /// Returns the principle signature type which this abstract domain
    /// implements.
    SigInstanceDecl *getPrincipleSignature() const {
        return *sigset.beginDirect();
    }

    static bool classof(const AbstractDomainDecl *node) { return true; }
    static bool classof(const Ast* node) {
        return node->getKind() == AST_AbstractDomainDecl;
    }

private:
    SignatureSet sigset;

    AstResource &getAstResource() {
        return getPrincipleSignature()->getSigoid()->getAstResource();
    }
};

//===----------------------------------------------------------------------===//
// DomainInstanceDecl
class DomainInstanceDecl : public DomainTypeDecl, public llvm::FoldingSetNode {

public:
    DomainInstanceDecl(DomainDecl *domain);

    DomainInstanceDecl(FunctorDecl *functor,
                       DomainTypeDecl **args, unsigned numArgs);

    /// Returns the Domoid defining this instance.
    Domoid *getDefinition() { return definition; }
    const Domoid *getDefinition() const { return definition; }

    /// If this is an instance of a domain, return the corresponding domain
    /// declaration.  Otherwise null is returned.
    DomainDecl *getDefiningDomain() const;

    /// If this is a functor instance, return the corresponding functor
    /// declaration.  Otherwise null is returned.
    FunctorDecl *getDefiningFunctor() const;

    /// Returns the SignatureSet of this instance.
    const SignatureSet &getSignatureSet() const { return sigset; }

    /// Returns true if this instance represents percent or is a parameterized
    /// instance, and in the latter case, if any of the arguments involve
    /// abstract domain decls or percent nodes.
    bool isDependent() const;

    /// Returns true if this is an instance of a functor.
    bool isParameterized() const { return getArity() != 0; }

    /// Returns the arity of the underlying declaration.
    unsigned getArity() const;

    /// Returns the i'th actual parameter.  This method asserts if its argument
    /// is out of range, or if this is not an instance of a functor.
    DomainTypeDecl *getActualParam(unsigned n) const {
        assert(isParameterized() && "Not a parameterized instance!");
        assert(n < getArity() && "Index out of range!");
        return arguments[n];
    }

    /// Returns the type of the i'th actual parameter.  This method asserts if
    /// its argument is out of range, or if this is not an instance of a
    /// functor.
    DomainType *getActualParamType(unsigned n) const {
        return getActualParam(n)->getType();
    }

    /// Iterators over the arguments supplied to this instance.
    typedef DomainTypeDecl **arg_iterator;
    arg_iterator beginArguments() const { return arguments; }
    arg_iterator endArguments() const { return &arguments[getArity()]; }

    /// Method required by LLVM::FoldingSet.
    void Profile(llvm::FoldingSetNodeID &id) {
        Profile(id, &arguments[0], getArity());
    }

    /// Called by FunctorDecl when memoizing.
    static void
    Profile(llvm::FoldingSetNodeID &id,
            DomainTypeDecl **args, unsigned numArgs);

    static bool classof(const DomainInstanceDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DomainInstanceDecl;
    }

private:
    Domoid *definition;
    DomainTypeDecl **arguments;
    SignatureSet sigset;

    // The following call-backs are invoked when the declarative region of the
    // defining declaration changes.
    void notifyAddDecl(Decl *decl);
    void notifyRemoveDecl(Decl *decl);
};

//===----------------------------------------------------------------------===//
// PercentDecl

class PercentDecl : public DomainTypeDecl {

public:
    /// Returns the model this PercentDecl represents.
    ModelDecl *getDefinition() { return underlyingModel; }
    const ModelDecl *getDefinition() const { return underlyingModel; }

    /// Returns the SignatureSet of this instance.
    const SignatureSet &getSignatureSet() const { return sigset; }

    static bool classof(const PercentDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PercentDecl;
    }

private:
    friend class ModelDecl;

    PercentDecl(AstResource &resource, ModelDecl *model);

    ModelDecl *underlyingModel;
    SignatureSet sigset;
};

//===----------------------------------------------------------------------===//
// Inline methods, now that the decl hierarchy is in place.

inline DomainType *ModelDecl::getPercentType() const
{
    return percent->getType();
}

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

inline DomainDecl *DomainInstanceDecl::getDefiningDomain() const
{
    return llvm::dyn_cast<DomainDecl>(definition);
}

inline FunctorDecl *DomainInstanceDecl::getDefiningFunctor() const
{
    return llvm::dyn_cast<FunctorDecl>(definition);
}

inline DomainType *SigInstanceDecl::getActualParamType(unsigned n) const {
    return getActualParam(n)->getType();
}

} // End comma namespace

#endif
