//===-- ast/Decl.h -------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DECL_HDR_GUARD
#define COMMA_AST_DECL_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/DeclRegion.h"
#include "comma/ast/Pragma.h"
#include "comma/ast/Type.h"
#include "comma/basic/ParameterModes.h"
#include "comma/basic/Pragmas.h"
#include "comma/basic/PrimitiveOps.h"

#include "llvm/ADT/ilist.h"
#include "llvm/ADT/FoldingSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/PointerUnion.h"

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
    DeclRegion *asDeclRegion() const;

    /// Returns the origin of this decl, or null if there is no associated
    /// origin.
    ///
    /// FIXME: Semantics need to be cleaned up.
    ///
    /// \see isImmediate
    Decl *getOrigin() { return origin; }
    const Decl *getOrigin() const { return origin; }

    /// Returns true if this decl has an origin.
    bool hasOrigin() const { return origin != 0; }

    /// Sets the origin of this decl.
    void setOrigin(Decl *decl) {
        assert(decl->getKind() == this->getKind() && "Kind mismatch!");
        origin = decl;
    }

    /// Walks the chain of origins returning the final non-null declaration;
    Decl *resolveOrigin();
    const Decl *resolveOrigin() const {
        return const_cast<Decl*>(this)->resolveOrigin();
    }

    /// Returns true if this declaration is immediate.
    ///
    /// FIXME: Semantics need to be cleaned up.
    bool isImmediate() const { return !hasOrigin(); }

    // Support isa and dyn_cast.
    static bool classof(const Decl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesDecl();
    }

protected:
    Decl(AstKind kind, IdentifierInfo *info = 0, Location loc = Location(),
         DeclRegion *region = 0)
        : Ast(kind),
          idInfo(info),
          location(loc),
          context(region),
          origin(0) {
        assert(this->denotesDecl());
        deletable = false;
    }

    IdentifierInfo *idInfo;
    Location location;
    DeclRegion *context;
    Decl *origin;
};

//===----------------------------------------------------------------------===//
// ExceptionDecl
//
/// \class
///
/// \brief ExceptionDecl's represent exception declarations.
///
/// Exceptions do not have a type.  They are effectively simple names attached
/// to an exception entity.
class ExceptionDecl : public Decl {

public:
    /// The following enumeration provides codes which map the the language
    /// defined primitive exception nodes, or to a user defined exception.
    enum ExceptionKind {
        User,                   ///< ID for user defined exceptions.
        Program_Error,          ///< Program_Error.
        Constraint_Error,       ///< Constraint_Error.
        Assertion_Error         ///< Assertion_Error
    };

    ExceptionKind getID() const { return static_cast<ExceptionKind>(bits); }

    // Support isa/dyn_cast.
    static bool classof(const ExceptionDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ExceptionDecl;
    }

private:
    ExceptionDecl(ExceptionKind ID, IdentifierInfo *name, Location loc,
                  DeclRegion *region)
        : Decl(AST_ExceptionDecl, name, loc, region) {
        bits = ID;
    }

    // Only AstResource is allowed to construct ExceptionDecl's.
    friend class AstResource;
};

//===----------------------------------------------------------------------===//
// PackageDecl
//
// Declaration node representing packages.
class PackageDecl : public Decl, public DeclRegion {

public:
    ~PackageDecl();

    PackageDecl(AstResource &resource, IdentifierInfo *name, Location loc);

    /// Returns the AstResource object associated with this package.
    ///
    /// This method is intended for use by other nodes in the AST, not by
    /// clients of the AST itself.
    AstResource &getAstResource() { return resource; }

    /// Returns true if this package has been associated with an implementation
    /// (body).
    bool hasImplementation() const { return implementation != 0; }

    //@{
    /// Returns the BodyDecl implementing this package, or null if a body has
    /// not yet been associated.
    BodyDecl *getImplementation() { return implementation; }
    const BodyDecl *getImplementation() const { return implementation; }
    //@}

    /// Returns true if this package has a private part.
    bool hasPrivatePart() const { return privateDeclarations != 0; }

    //@{
    /// Returns the PrivatePart node encapsulating the private declarations of
    /// this package, or null if a private part has not yet been associated.
    PrivatePart *getPrivatePart() { return privateDeclarations; }
    const PrivatePart *getPrivatePart() const { return privateDeclarations; }
    //@}

    //@{
    /// Returns the instance corresponding to this package.
    PkgInstanceDecl *getInstance();
    const PkgInstanceDecl *getInstance() const {
        return const_cast<PackageDecl*>(this)->getInstance();
    }
    //@}

    static bool classof(const PackageDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PackageDecl;
    }

private:
    AstResource &resource;
    BodyDecl *implementation;
    PrivatePart *privateDeclarations;
    PkgInstanceDecl *instance;

    /// Make BodyDecl and PrivatePart friends.  BodyDecl is permitted to call
    /// setImplementation in order to register itself with its package, likewise
    /// PrivatePart may call setPrivatePart.
    friend class BodyDecl;
    friend class PrivatePart;

    /// Sets the implementation of this package.  Once set, a package
    /// implementation cannot be reset.
    void setImplementation(BodyDecl *body);

    /// Sets the private part of this package.  This method if for use by a
    /// PrivatePart node can only be called once.
    void setPrivatePart(PrivatePart *privateDeclarations);
};

//===----------------------------------------------------------------------===//
/// \class PrivatePart
///
/// \brief Encapsulates the private declarations of a package.
class PrivatePart : public Ast, public DeclRegion
{
public:
    /// Creates a PrivateDecl to represent the private declarations of the given
    /// package.  Upon construction, a PrivateDecl automatically registers
    /// itself with the given package.
    PrivatePart(PackageDecl *package, Location loc);

    /// Returns the location of the "private" keyword.
    Location getLocation() const { return loc; }

    //@{
    /// Returns the package to which this PrivateDecl belongs.
    const PackageDecl *getPackage() const {
        return llvm::cast<PackageDecl>(getParent());
    }
    PackageDecl *getPackage() {
        return llvm::cast<PackageDecl>(getParent());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const PrivatePart *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PrivatePart;
    }

private:
    Location loc;
};

//===----------------------------------------------------------------------===//
// BodyDecl
//
/// \class BodyDecl
///
/// This class represents the body (implementation) of a package.
class BodyDecl : public Decl, public DeclRegion {

public:
    /// Creates a BodyDecl to represent the body of the given package.
    BodyDecl(PackageDecl *package, Location loc);

    /// Returns the location of this BodyDecl.
    Location getLocation() const { return loc; }

    //@{
    /// Returns the package which this body implements.
    const PackageDecl *getPackage() const {
        return const_cast<BodyDecl*>(this)->getPackage();
    }
    PackageDecl *getPackage();
    //@}

    // Support isa/dyn_cast.
    static bool classof(const BodyDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_BodyDecl;
    }

private:
    Location loc;
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
    //@{
    /// \brief Returns the type of this value declaration.
    const Type *getType() const { return correspondingType; }
    Type *getType() { return correspondingType; }
    //@}

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
// RenamedObjectDecl
//
/// \class
///
/// A RenamedObjectDecl represents renamed object declarations.
class RenamedObjectDecl : public ValueDecl {

public:
    RenamedObjectDecl(IdentifierInfo *name, Type *type, Location loc,
                      Expr *expr)
        : ValueDecl(AST_RenamedObjectDecl, name, type, loc),
          renamedExpr(expr) { }

    //@{
    /// Returns the expression which this declaration renames.
    const Expr *getRenamedExpr() const { return renamedExpr; }
    Expr *getRenamedExpr() { return renamedExpr; }
    //@}

    /// Sets the expression this declaration renames.
    void setRenamedExpr(Expr *expr) { renamedExpr = expr; }

    // Support isa/dyn_cast.
    static bool classof(const RenamedObjectDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_RenamedObjectDecl;
    }

private:
    Expr *renamedExpr;
};

//===----------------------------------------------------------------------===//
// LoopDecl
//
/// These specialized nodes represent the iteration variable in a for loop.
class LoopDecl : public ValueDecl {

public:
    LoopDecl(IdentifierInfo *name, DiscreteType *type, Location loc)
        : ValueDecl(AST_LoopDecl, name, type, loc) { }

    //@{
    /// Specialize ValueDecl::getType().
    const DiscreteType *getType() const {
        return llvm::cast<DiscreteType>(ValueDecl::getType());
    }
    DiscreteType *getType() {
        return llvm::cast<DiscreteType>(ValueDecl::getType());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const LoopDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_LoopDecl;
    }
};

//===----------------------------------------------------------------------===//
// SubroutineDecl
//
// Base class for representing procedures and functions.
class SubroutineDecl : public Decl, public DeclRegion {

public:
    virtual ~SubroutineDecl();

    //@{
    /// Returns the type of this declaration.
    virtual SubroutineType *getType() = 0;
    virtual const SubroutineType *getType() const = 0;
    //@}

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

    //@{
    /// Specialization of Decl::getOrigin().
    SubroutineDecl *getOrigin() {
        return llvm::cast_or_null<SubroutineDecl>(Decl::getOrigin());
    }
    const SubroutineDecl *getOrigin() const {
        return llvm::cast_or_null<SubroutineDecl>(Decl::getOrigin());
    }
    //@}

    //@{
    /// Specialization of Decl::resolveOrigin().
    SubroutineDecl *resolveOrigin() {
        return llvm::cast_or_null<SubroutineDecl>(Decl::resolveOrigin());
    }
    const SubroutineDecl *resolveOrigin() const {
        return llvm::cast_or_null<SubroutineDecl>(Decl::resolveOrigin());
    }
    //@}

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

    bool hasDefiningDeclaration() const {
        return getDefiningDeclaration() != 0;
    }

    SubroutineDecl *getDefiningDeclaration() {
        if (declarationLink.getInt() == DEFINITION_TAG)
            return declarationLink.getPointer();
        return 0;
    }

    const SubroutineDecl *getDefiningDeclaration() const {
        if (declarationLink.getInt() == DEFINITION_TAG)
            return declarationLink.getPointer();
        return 0;
    }

    SubroutineDecl *getForwardDeclaration() {
        if (declarationLink.getInt() == FORWARD_TAG)
            return declarationLink.getPointer();
        return 0;
    }

    const SubroutineDecl *getForwardDeclaration() const {
        if (declarationLink.getInt() == FORWARD_TAG)
            return declarationLink.getPointer();
        return 0;
    }

    const bool hasForwardDeclaration() const {
        return getForwardDeclaration() != 0;
    }

    const bool isForwardDeclaration() const {
        return getDefiningDeclaration() != 0;
    }

    bool hasBody() const;
    void setBody(BlockStmt *block) { body = block; }
    BlockStmt *getBody();
    const BlockStmt *getBody() const {
        return const_cast<SubroutineDecl*>(this)->getBody();
    }

    /// Returns true if this subroutine represents a primitive operation.
    bool isPrimitive() const { return opID != PO::NotPrimitive; }

    /// Marks this declaration as primitive.
    void setAsPrimitive(PO::PrimitiveID ID) { opID = ID; }

    /// Returns the PrimitiveID of this subroutine.
    PO::PrimitiveID getPrimitiveID() const { return opID; }

    /// Associates the given pragma with this declaration.  The declaration node
    /// takes ownership of the supplied pragma.
    void attachPragma(Pragma *P) { pragmas.push_front(P); }

    /// Locates a pragma with the given ID.
    ///
    /// This method traverses the set of pragmas associated with this
    /// declaration, as well as any pragmas associated with a related
    /// declaration (forward, defining, or origin).  To scan the set of pragmas
    /// associated with this declaration and this declaration alone use a
    /// pragma_iterator instead.
    const Pragma *findPragma(pragma::PragmaID ID) const;

    /// Returns true if a pragma with the given ID has been assoiated with this
    /// declaration.
    bool hasPragma(pragma::PragmaID ID) const { return findPragma(ID) != 0; }

    //@{
    /// Iterators over the attached set of pragmas.
    typedef llvm::iplist<Pragma>::iterator pragma_iterator;
    pragma_iterator begin_pragmas() { return pragmas.begin(); }
    pragma_iterator end_pragmas() { return pragmas.end(); }

    typedef llvm::iplist<Pragma>::const_iterator const_pragma_iterator;
    const_pragma_iterator begin_pragmas() const { return pragmas.begin(); }
    const_pragma_iterator end_pragmas() const { return pragmas.end(); }
    //@}

    /// Compares the profiles of two subroutine declarations for equality.
    ///
    /// \see SubroutineType::compareProfiles.
    static bool compareProfiles(const SubroutineDecl *X,
                                const SubroutineDecl *Y) {
        return SubroutineType::compareProfiles(X->getType(), Y->getType());
    }

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

    SubroutineDecl(AstKind kind, IdentifierInfo *name, Location loc,
                   DeclRegion *parent);

    PO::PrimitiveID opID : 8;   ///< Identifies the type of operation.

    unsigned numParameters;
    ParamValueDecl **parameters;
    BlockStmt *body;

    /// Enumeration used to tag the declaration link.
    enum DeclLinkTag {
        FORWARD_TAG,            // Link points to a forward declaration.
        DEFINITION_TAG          // Link points to a completion.
    };

    llvm::PointerIntPair<SubroutineDecl*, 1, DeclLinkTag> declarationLink;
    llvm::iplist<Pragma> pragmas;

private:
    /// Helper for findPragma.  Scans only this declarations pragma list and not
    /// those associated with a forward declaration or completion.
    const Pragma *locatePragma(pragma::PragmaID ID) const;
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

    //@{
    /// \brief Returns the type of this procedure declaration.
    const ProcedureType *getType() const { return correspondingType; }
    ProcedureType *getType() { return correspondingType; }
    //@}

    ProcedureDecl *getDefiningDeclaration() {
        SubroutineDecl *definition = SubroutineDecl::getDefiningDeclaration();
        return llvm::cast_or_null<ProcedureDecl>(definition);
    }

    const ProcedureDecl *getDefiningDeclaration() const {
        const SubroutineDecl *definition;
        definition = SubroutineDecl::getDefiningDeclaration();
        return llvm::cast_or_null<ProcedureDecl>(definition);
    }

    ProcedureDecl *getForwardDeclaration() {
        SubroutineDecl *forward = SubroutineDecl::getForwardDeclaration();
        return llvm::cast_or_null<ProcedureDecl>(forward);
    }

    const ProcedureDecl *getForwardDeclaration() const {
        const SubroutineDecl *forward;
        forward = SubroutineDecl::getForwardDeclaration();
        return llvm::cast_or_null<ProcedureDecl>(forward);
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

    //@{
    /// \brief Returns the type of this function declaration.
    const FunctionType *getType() const { return correspondingType; }
    FunctionType *getType() { return correspondingType; }
    //@}

    FunctionDecl *getDefiningDeclaration() {
        SubroutineDecl *definition = SubroutineDecl::getDefiningDeclaration();
        return llvm::cast_or_null<FunctionDecl>(definition);
    }

    const FunctionDecl *getDefiningDeclaration() const {
        const SubroutineDecl *definition;
        definition = SubroutineDecl::getDefiningDeclaration();
        return llvm::cast_or_null<FunctionDecl>(definition);
    }

    FunctionDecl *getForwardDeclaration() {
        SubroutineDecl *forward = SubroutineDecl::getForwardDeclaration();
        return llvm::cast_or_null<FunctionDecl>(forward);
    }

    const FunctionDecl *getForwardDeclaration() const {
        const SubroutineDecl *forward;
        forward = SubroutineDecl::getForwardDeclaration();
        return llvm::cast_or_null<FunctionDecl>(forward);
    }

    //@{
    /// \brief Provides the return type of this function.
    const Type *getReturnType() const { return getType()->getReturnType(); }
    Type *getReturnType() { return getType()->getReturnType(); }
    //@}

    // Support for isa and dyn_cast.
    static bool classof(const FunctionDecl *node) { return true; }
    static bool classof(const Ast *node) { return denotesFunctionDecl(node); }

protected:
    // Constructor for use by EnumLiteral.
    FunctionDecl(AstKind kind, AstResource &resource,
                 IdentifierInfo *name, Location loc,
                 EnumerationType *returnType, DeclRegion *parent);

    // Constructor for use by FunctionAttribDecl.
    FunctionDecl(AstKind kind, IdentifierInfo *name, Location loc,
                 IdentifierInfo **keywords, FunctionType *type,
                 DeclRegion *parent)
        : SubroutineDecl(kind, name, loc, keywords, type, parent),
          correspondingType(type) { }

private:
    FunctionType *correspondingType;

    void initializeCorrespondingType(AstResource &resource, Type *returnType);
    static bool denotesFunctionDecl(const Ast *node);
};

//===----------------------------------------------------------------------===//
// EnumLiteral
//
// Instances of this class represent the elements of an EnumerationDecl.
class EnumLiteral : public FunctionDecl {

public:
    /// Returns the index (or value) of this EnumLiteral.
    unsigned getIndex() const { return index; }

    //@{
    /// Provides the return type of this EnumLiteral.
    const EnumerationType *getReturnType() const {
        return llvm::cast<EnumerationType>(FunctionDecl::getReturnType());
    }
    EnumerationType *getReturnType() {
        return llvm::cast<EnumerationType>(FunctionDecl::getReturnType());
    }
    //@}

    //@{
    /// Returns the EnumerationDecl this literal belongs to.
    EnumerationDecl *getDeclRegion() {
        return llvm::cast<EnumerationDecl>(context);
    }
    const EnumerationDecl *getDeclRegion() const {
        return llvm::cast<EnumerationDecl>(context);
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const EnumLiteral *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumLiteral;
    }

private:
    // Enumeration literals are constructed by their containing enumeration decl
    // node.
    EnumLiteral(AstResource &resource, IdentifierInfo *name, Location loc,
                unsigned index, EnumerationType *type, EnumerationDecl *parent);

    friend class EnumerationDecl;

    unsigned index;
};

//===----------------------------------------------------------------------===//
// TypeDecl
//
// All nodes which declare types inherit from this class.
class TypeDecl : public Decl {

public:
    //@{
    /// \brief Returns the type defined by this type declaration.
    const PrimaryType *getType() const { return CorrespondingType; }
    PrimaryType *getType() { return CorrespondingType; }
    //@}

    /// Populates the declarative region of this type with all implicit
    /// operations.  This must be called once the type has been constructed to
    /// gain access to the types operations.
    virtual void generateImplicitDeclarations(AstResource &resource);

    /// Returns true if this denotes a subtype declarations.
    virtual bool isSubtypeDeclaration() const = 0;

    // Support isa/dyn_cast.
    static bool classof(const TypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesTypeDecl();
    }

protected:
    // Constructs a TypeDecl node when a type is immediately available.
    TypeDecl(AstKind kind, IdentifierInfo *name, PrimaryType *type,
             Location loc, DeclRegion *region = 0)
        : Decl(kind, name, loc, region),
          CorrespondingType(type) {
        assert(this->denotesTypeDecl());
    }

    // Constructs a TypeDecl node when a type is not immediately available.
    // Users of this constructor must set the corresponding type.
    TypeDecl(AstKind kind, IdentifierInfo *name, Location loc,
             DeclRegion *region = 0)
        : Decl(kind, name, loc, region),
          CorrespondingType(0) {
        assert(this->denotesTypeDecl());
    }

    PrimaryType *CorrespondingType;
};

//===----------------------------------------------------------------------===//
// IncompleteTypeDecl
//
/// \class
///
/// \brief Represents the occurence of an incomplete type declaration.
class IncompleteTypeDecl : public TypeDecl {

public:
    /// \brief Returns true if this declaration has a completion.
    bool hasCompletion() const { return completion != 0; }

    /// \brief Sets the completion of this declaration.
    void setCompletion(TypeDecl *decl) { completion = decl; }

    //@{
    /// Specialize TypeDecl::getType().
    const IncompleteType *getType() const {
        return llvm::cast<IncompleteType>(CorrespondingType);
    }
    IncompleteType *getType() {
        return llvm::cast<IncompleteType>(CorrespondingType);
    }
    //@}

    //@{
    /// \brief Returns the completion of this declaration if one has been set,
    /// else null.
    const TypeDecl *getCompletion() const { return completion; }
    TypeDecl *getCompletion() { return completion; }
    //@}

    /// \brief Returns true if the given declaration could serve as a
    /// completion.
    ///
    /// A declaration can complete this incomplete type declaration if:
    ///
    ///   - This declaration is itself without a completion,
    ///
    ///   - If the given declaration and this one are declared in the same
    ///     declarative region, or
    ///
    ///   - This declaration was declared in the public portion of a package and
    ///     the given declaration was declared in the private portion.
    bool isCompatibleCompletion(const TypeDecl *decl) const;

    /// Returns true if the completion of this indirect type declaration
    /// is visible from within the given declarative region.
    bool completionIsVisibleIn(const DeclRegion *region) const;

    bool isSubtypeDeclaration() const { return false; }

    // Support isa/dyn_cast.
    static bool classof(const IncompleteTypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IncompleteTypeDecl;
    }

private:
    // Constructs an incomplete type declaration node.
    IncompleteTypeDecl(AstResource &resource,
                       IdentifierInfo *name, Location loc, DeclRegion *region);

    // Incomplete type declarations are constructed an managed by AstResource.
    friend class AstResource;

    TypeDecl *completion;
};

//===----------------------------------------------------------------------===//
// EnumerationDecl
class EnumerationDecl : public TypeDecl, public DeclRegion {

    /// This enumeration provides encodings for various flags which are munged
    /// into the bits field of this node.
    enum PropertyFlag {
        Subtype_FLAG   = 1 << 0, // Set when this is a subtype decl.
        Character_FLAG = 1 << 1  // Set when this is a character decl.
    };

public:
    /// Returns true if this declaration denotes a subtype declaration.
    bool isSubtypeDeclaration() const { return bits & Subtype_FLAG; }

    /// Populates the declarative region of this type with all implicit
    /// operations.  This must be called once the type has been constructed to
    /// gain access to the types operations.
    void generateImplicitDeclarations(AstResource &resource);

    //@{
    /// \brief Returns the first subtype of this enumeration type declaration.
    const EnumerationType *getType() const {
        return llvm::cast<EnumerationType>(CorrespondingType);
    }
    EnumerationType *getType() {
        return llvm::cast<EnumerationType>(CorrespondingType);
    }
    //@}

    //@{
    /// \brief Returns the base subtype of this enumeration type declaration.
    const EnumerationType *getBaseSubtype() const {
        return getType()->getRootType()->getBaseSubtype();
    }
    EnumerationType *getBaseSubtype() {
        return getType()->getRootType()->getBaseSubtype();
    }
    //@}

    // Returns the number of EnumLiteral's associated with this enumeration.
    unsigned getNumLiterals() const { return numLiterals; }

    // Returns the minimum number of bits needed to represent elements of this
    // enumeration.
    unsigned genBitsNeeded() const;

    // Returns the literal with the given name, or null if no such literal is a
    // member of this enumeration.
    EnumLiteral *findLiteral(IdentifierInfo *name);

    //@{
    /// Returns the first enumeration literal defined by this declaration.
    const EnumLiteral *getFirstLiteral() const;
    EnumLiteral *getFirstLiteral();
    //@}

    //@{
    /// Returns the last enumeration literal defined by this declaration.
    const EnumLiteral *getLastLiteral() const;
    EnumLiteral *getLastLiteral();
    //@}

    // Marks this declaration as a character enumeration.
    //
    // This method should be called if any of the literals constituting this
    // declaration are character literals.
    void markAsCharacterType() { bits |= Character_FLAG; }

    // Returns true if this declaration denotes a character enumeration.
    bool isCharacterType() const { return bits & Character_FLAG; }

    // Locates the EnumLiteral corresponding to the given character, or null if
    // no such literal exists.
    const EnumLiteral *findCharacterLiteral(char ch) const;

    // Returns true if an enumeration literal exists which maps to the given
    // character.
    bool hasEncoding(char ch) const {
        return findCharacterLiteral(ch) != 0;
    }

    // Returns the encoding of the given character.  This method is only valid
    // if hasEncoding() returns true for the given character.
    unsigned getEncoding(char ch) const {
        const EnumLiteral *lit = findCharacterLiteral(ch);
        assert(lit && "No encoding exists for the given character!");
        return lit->getIndex();
    }

    /// Returns the declaration corresponding to the Pos attribute.
    PosAD *getPosAttribute() { return posAttribute; }

    /// Returns the declaration corresponding to the Val attribute.
    ValAD *getValAttribute() { return valAttribute; }

    /// Returns a Pos or Val attribute corresponding to the given ID.
    ///
    /// If the given ID does not correspond to an attribute defined for this
    /// type an assertion will fire.
    FunctionAttribDecl *getAttribute(attrib::AttributeID ID);

    static bool classof(const EnumerationDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_EnumerationDecl;
    }

private:
    // Private constructors for use by AstResource.

    /// Constructs an enumeration declaration.
    ///
    /// \param elems A sequence of pairs giving the defining identifier and
    /// location for each enumeration literal provided by this declaration.
    EnumerationDecl(AstResource &resource,
                    IdentifierInfo *name, Location loc,
                    std::pair<IdentifierInfo*, Location> *elems,
                    unsigned numElems, DeclRegion *parent);

    /// Constructs an unconstrained enumeration subtype declaration.
    EnumerationDecl(AstResource &resource, IdentifierInfo *name, Location loc,
                    EnumerationType *subtype, DeclRegion *region);

    /// Constructs a constrained enumeration subtype declaration.
    EnumerationDecl(AstResource &resource, IdentifierInfo *name, Location loc,
                    EnumerationType *subtype, Expr *lower, Expr *upper,
                    DeclRegion *region);

    friend class AstResource;

    // Generates the implicit declarations that must be attached to the
    // primitive Boolean type and that type alone.  This method is for use by
    // AstResource when it establishes the fundamental type nodes.
    void generateBooleanDeclarations(AstResource &resource);

    // The number of EnumLiteral's associated with this enumeration.
    uint32_t numLiterals;

    PosAD *posAttribute;        // Declaration node for the Pos attribute.
    ValAD *valAttribute;        // Declaration node for the Val attribute.
};

//===----------------------------------------------------------------------===//
// IntegerDecl
//
// These nodes represent integer type declarations.
class IntegerDecl : public TypeDecl, public DeclRegion {

    /// The following flags are or'ed together into the Ast::bits field
    /// indicating if this is a subtype and/or modular type declaration.
    enum IDeclFlags {
        Subtype_FLAG = 1 << 0,
        Modular_FLAG = 1 << 1
    };

public:
    /// Returns true if this declaration declares an integer subtype.
    bool isSubtypeDeclaration() const { return bits & Subtype_FLAG; }

    /// Returns true if this declaration declares a modular type.
    bool isModularDeclaration() const { return bits & Modular_FLAG; }

    /// Populates the declarative region of this type with all implicit
    /// operations.  This must be called once the type has been constructed to
    /// gain access to the types operations.
    void generateImplicitDeclarations(AstResource &resource);

    //@{
    /// \brief Returns the subtype defined by this integer declaration.
    ///
    /// This method returns the first subtype of this declaration, or in the
    /// special case of root_integer, the unconstrained base subtype.
    const IntegerType *getType() const {
        return llvm::cast<IntegerType>(CorrespondingType);
    }
    IntegerType *getType() {
        return llvm::cast<IntegerType>(CorrespondingType);
    }
    //@}

    //@{
    /// \brief Returns the base subtype of this integer type declaration.
    const IntegerType *getBaseSubtype() const {
        return getType()->getRootType()->getBaseSubtype();
    }
    IntegerType *getBaseSubtype() {
        return getType()->getRootType()->getBaseSubtype();
    }
    //@}

    //@{
    /// Returns the expression denoting the lower bound of this integer
    /// declaration.  If this denotes an unconstrained subtype declaration or a
    /// modular type declaration this method returns null.
    Expr *getLowBoundExpr();
    const Expr *getLowBoundExpr() const {
        return const_cast<IntegerDecl*>(this)->getLowBoundExpr();
    }
    //@}

    //@{
    /// Returns the expression denoting the upper bound of this integer
    /// declaration.  If this denotes an unconstrained subtype declaration or a
    //modular type declaration this method returns null.
    Expr *getHighBoundExpr();
    const Expr *getHighBoundExpr() const {
        return const_cast<IntegerDecl*>(this)->getHighBoundExpr();
    }
    //@}

    //@{
    /// If this is a modular integer type declaration return the modulus, else
    /// null.
    Expr *getModulusExpr();
    const Expr *getModulusExpr() const {
        return const_cast<IntegerDecl*>(this)->getModulusExpr();
    }
    //@}


    /// Returns the declaration corresponding to the Pos attribute.
    PosAD *getPosAttribute() { return posAttribute; }

    /// Returns the declaration corresponding to the Val attribute.
    ValAD *getValAttribute() { return valAttribute; }

    /// Returns a Pos or Val attribute corresponding to the given ID.
    ///
    /// If the given ID does not correspond to an attribute defined for this
    /// type an assertion will fire.
    FunctionAttribDecl *getAttribute(attrib::AttributeID ID);

    // Support isa/dyn_cast.
    static bool classof(const IntegerDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_IntegerDecl;
    }

private:
    /// Constructs an Integer type declaration (for use by AstResource).
    ///
    /// \p resource A reference to the AstResource which is performing the
    ///    allocation.
    ///
    /// \p name IdentifierInfo naming the declaration.
    ///
    /// \p loc Location denoting the position of the declarations name in the
    ///    source.
    ///
    /// \p lower Expression denoting the lower bound of this integer type.
    ///
    /// \p upper Expression denoting the upper bound of this integer type.
    ///
    /// \p parent The declarative region in which the integer type declaration
    ///    appears.
    IntegerDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                Expr *lower, Expr *upper, DeclRegion *parent);

    /// Creates a modular Integer type declaration.
    IntegerDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                Expr *modulus, DeclRegion *parent);


    /// Constructs an unconstrained integer subtype declaration.
    IntegerDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                IntegerType *subtype, DeclRegion *parent);

    /// Constructs a constrained integer subtype declaration.
    IntegerDecl(AstResource &resource,
                IdentifierInfo *name, Location loc,
                IntegerType *subtype,
                Expr *lower, Expr *upper, DeclRegion *parent);

    friend class AstResource;

    Expr *lowExpr;              // Expr forming the lower bound or modulus.
    Expr *highExpr;             // Expr forming the high bound.

    PosAD *posAttribute;        // Declaration node for the Pos attribute.
    ValAD *valAttribute;        // Declaration node for the Val attribute.
};

//===----------------------------------------------------------------------===//
// ArrayDecl
//
// This node represents array type declarations.
class ArrayDecl : public TypeDecl, public DeclRegion {

    // Type used to store the DSTDefinitions defining the indices of this array.
    typedef llvm::SmallVector<DSTDefinition*, 4> IndexVec;

public:
    //@{
    /// \brief Returns the first subtype defined by this array type declaration.
    const ArrayType *getType() const {
        return llvm::cast<ArrayType>(CorrespondingType);
    }
    ArrayType *getType() {
        return llvm::cast<ArrayType>(CorrespondingType);
    }
    //@}

    /// Returns the rank of this array declaration.
    unsigned getRank() const { return getType()->getRank(); }

    //@{
    /// \brief Returns the type describing the i'th index of this array.
    const DiscreteType *getIndexType(unsigned i) const {
        return getType()->getIndexType(i);
    }
    DiscreteType *getIndexType(unsigned i) {
        return getType()->getIndexType(i);
    }
    //@}

    //@{
    /// \brief Returns the type describing the component type of this array.
    const Type *getComponentType() const {
        return getType()->getComponentType();
    }
    Type *getComponentType() { return getType()->getComponentType(); }
    //@}

    //@{
    /// Returns the discrete subtype definition node for the i'th index.
    ///
    /// Reminder: Discrete subtype definitions provide information about the
    /// source syntax used to declare the index.  Consequently, they do not
    /// provide any additional semantic information about the index type.
    const DSTDefinition *getDSTDefinition(unsigned i) const {
        return indices[i];
    }
    DSTDefinition *getDSTDefinition(unsigned i) { return indices[i]; }
    //@}

    /// Returns true if this declaration is constrained.
    bool isConstrained() const { return getType()->isConstrained(); }

    bool isSubtypeDeclaration() const;

    //@{
    /// Iterators over the the index types of this array.
    typedef ArrayType::iterator index_iterator;
    index_iterator begin_indices() { return getType()->begin(); }
    index_iterator end_indices() { return getType()->end(); }
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
              unsigned rank, DSTDefinition **indices,
              Type *component, bool isConstrained, DeclRegion *parent);

    friend class AstResource;

    IndexVec indices;
};

//===----------------------------------------------------------------------===//
// RecordDecl
//
class RecordDecl : public TypeDecl, public DeclRegion {

public:
    //@{
    /// \brief Returns this first subtype defined by this record type
    /// declaration.
    const RecordType *getType() const {
        return llvm::cast<RecordType>(CorrespondingType);
    }
    RecordType *getType() {
        return llvm::cast<RecordType>(CorrespondingType);
    }
    //@}

    /// Constructs a ComponentDecl and associates it with this record.
    ///
    /// The order in which is method is called determines the order of the
    /// associated components.
    ComponentDecl *addComponent(IdentifierInfo *name, Location loc,
                                Type *type);

    /// Returns the number of components provided by this record.
    unsigned numComponents() const { return componentCount; }

    //@{
    /// \brief Returns the component declaration with the given identifier.
    ///
    /// Returns a null if a component with the given name could not be found.
    const ComponentDecl *getComponent(IdentifierInfo *name) const {
        return const_cast<RecordDecl*>(this)->getComponent(name);
    }
    ComponentDecl *getComponent(IdentifierInfo *name);
    //@}

    //@{
    /// Returns the i'th component declaration provided by this record.
    const ComponentDecl *getComponent(unsigned i) const {
        return const_cast<RecordDecl*>(this)->getComponent(i);
    }
    ComponentDecl *getComponent(unsigned i);
    //@}

    bool isSubtypeDeclaration() const;

    static bool classof(const RecordDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_RecordDecl;
    }

private:
    /// Constructs an empty record declaration.
    RecordDecl(AstResource &resource, IdentifierInfo *name, Location loc,
               DeclRegion *parent);

    friend class AstResource;

    // Total number of components defined by this record.
    unsigned componentCount;
};

//===----------------------------------------------------------------------===//
// ComponentDecl
//
/// \class
///
/// \brief Declaration node representing a record component.
class ComponentDecl : public Decl {

public:
    //@{
    /// \brief Returns the type of this component.
    const Type *getType() const { return CorrespondingType; }
    Type *getType() { return CorrespondingType; }
    //@}

    //@{
    /// \brief Returns the RecordDecl this component belongs to.
    RecordDecl *getDeclRegion() {
        return llvm::cast<RecordDecl>(context);
    }
    const RecordDecl *getDeclRegion() const {
        return llvm::cast<RecordDecl>(context);
    }
    //@}

    /// \brief Returns the index of this component.
    ///
    /// The value returned by this method gives the relative position of the
    /// component within its inclosing record.  In particular,
    /// RecordDecl::getComponent(this->getIndex()) == this.
    unsigned getIndex() const { return index; }

    // Support isa/dyn_cast.
    static bool classof(const ComponentDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ComponentDecl;
    }

private:
    // ComponentDecl's are constructed by their enclosing record declaration.
    ComponentDecl(IdentifierInfo *name, Location loc,
                  Type *type, unsigned index, RecordDecl *parent)
        : Decl(AST_ComponentDecl, name, loc, parent),
          CorrespondingType(type), index(index) { }

    friend class RecordDecl;

    Type *CorrespondingType;    // Type of this component.
    unsigned index;             // Relative position of this component.
};

//===----------------------------------------------------------------------===//
// AccessDecl
//
/// \class
///
/// \brief This class encapsulates an access type declaration.
class AccessDecl : public TypeDecl, public DeclRegion
{
    // Various flags which are munged into the bits member of this node.
    enum {
        Subtype_FLAG = 1 << 0
    };

public:
    /// Populates the declarative region of this type with all implicit
    /// operations.  This must be called once the type has been constructed to
    /// gain access to the types operations.
    void generateImplicitDeclarations(AstResource &resource);

    //@{
    /// \brief Returns the first subtype defined by this access type
    /// declaration.
    const AccessType *getType() const {
        return llvm::cast<AccessType>(CorrespondingType);
    }
    AccessType *getType() {
        return llvm::cast<AccessType>(CorrespondingType);
    }
    //@}

    bool isSubtypeDeclaration() const;

    // Support isa/dyn_cast.
    static bool classof(const AccessDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_AccessDecl;
    }

private:
    AccessDecl(AstResource &resource, IdentifierInfo *name, Location loc,
               Type *targetType, DeclRegion *parent);

    AccessDecl(AstResource &resource, IdentifierInfo *name, Location loc,
               AccessType *baseType, DeclRegion *parent);

    // AccessDecl's are constructed and managed by AstResource.
    friend class AstResource;
};

//===----------------------------------------------------------------------===//
/// \class PrivateTypeDecl
///
/// \brief Represents a private type declaration.
class PrivateTypeDecl : public TypeDecl, public DeclRegion
{
public:
    enum TypeTag {
        Abstract = 1 << 0,
        Tagged   = 1 << 1,
        Limited  = 1 << 2
    };

    PrivateTypeDecl(AstResource &resource, IdentifierInfo *name, Location loc,
                    unsigned tags, DeclRegion *context);

    /// \brief Returns true if the given declaration could serve as a
    /// completion.
    ///
    /// A declaration can complete this private type declaration if:
    ///
    ///   - This declaration is itself without a completion,
    ///
    ///   - If the given declaration appears in the public part of a package and
    ///     this declaration appears in the private part of the same package.
    bool isCompatibleCompletion(const TypeDecl *decl) const;

    //@{
    /// \brief Returns the completion of this declaration if one has been set,
    /// else null.
    const TypeDecl *getCompletion() const { return completion; }
    TypeDecl *getCompletion() { return completion; }
    //@}

    /// Returns true if this declaration has a completion.
    bool hasCompletion() const { return completion != 0; }

    /// \brief Sets the completion of this declaration.
    void setCompletion(TypeDecl *decl) { completion = decl; }

    /// Returns true if the completion of this private type declaration is
    /// visible from within the given declarative region.
    bool completionIsVisibleIn(const DeclRegion *region) const;

    void generateImplicitDeclarations(AstResource &resource);

    /// Returns a bitmask of type tags that have been applied to this
    /// declaration.
    unsigned getTypeTags() const { return bits; }

    /// Returns true if this is a limited type declaration.
    bool isLimited() const { return bits & Limited; }

    /// Returns true if this is an abstract type declaration.
    bool isAbstract() const { return bits & Abstract; }

    /// Returns true if this is a tagged type declaration.
    bool isTagged() const { return bits & Tagged; }

    bool isSubtypeDeclaration() const { return false; }

    // Support isa/dyn_cast.
    static bool classof(const PrivateTypeDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PrivateTypeDecl;
    }

private:
    TypeDecl *completion;
};

//===----------------------------------------------------------------------===//
// PkgInstanceDecl

class PkgInstanceDecl : public Decl, public DeclRegion {

public:
    // FIXME: Perhaps package instances should have a decl region corresponding
    // to the point of instantiation.
    PkgInstanceDecl(IdentifierInfo *name, Location loc, PackageDecl *package);

    //@{
    /// Returns the package declaration defining this instance.
    PackageDecl *getDefinition() { return definition; }
    const PackageDecl *getDefinition() const { return definition; }
    //@}

    static bool classof(const PkgInstanceDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PkgInstanceDecl;
    }

private:
    PackageDecl *definition;
};

//===----------------------------------------------------------------------===//
// UseDecl
//
// Represents using declarations.
class UseDecl : public Decl {

public:
    UseDecl(PkgInstanceDecl *target, Location loc)
        : Decl(AST_UseDecl, 0, loc),
          target(target) { }

    Decl *getUsedDecl() { return target; }
    const Decl *getUsedDecl() const { return target; }

    static bool classof(const UseDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_UseDecl;
    }

private:
    Decl *target;
};

} // End comma namespace

#endif
