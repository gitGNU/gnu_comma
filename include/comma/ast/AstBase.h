//===-- ast/AstBase.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// This header contains forwards declarations for all AST nodes, and definitions
// for a few choice fundamental nodes and types.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTBASE_HDR_GUARD
#define COMMA_AST_ASTBASE_HDR_GUARD

#include "comma/basic/Location.h"
#include "comma/basic/IdentifierInfo.h"
#include "llvm/Support/Casting.h"
#include <map>
#include <list>
#include <iosfwd>

namespace comma {

//
// Forward declarations for all Ast nodes.
//
class AbstractDomainDecl;
class AddDecl;
class Ast;
class AstRewriter;
class BlockStmt;
class CarrierDecl;
class CompilationUnit;
class Decl;
class DeclarativeRegion;
class DeclRefExpr;
class DomainDecl;
class DomainInstanceDecl;
class DomainType;
class Domoid;
class Expr;
class FunctionCallExpr;
class FunctionDecl;
class FunctionType;
class FunctorDecl;
class FunctorType;
class KeywordSelector;
class ModelDecl;
class ModelType;
class NamedDecl;
class ObjectDecl;
class ParameterizedModel;
class ParameterizedType;
class ParamValueDecl;
class ProcedureCallStmt;
class ProcedureDecl;
class ProcedureType;
class Sigoid;
class SignatureDecl;
class SignatureType;
class Stmt;
class SubroutineDecl;
class SubroutineType;
class Type;
class TypeDecl;
class ValueDecl;
class VarietyDecl;
class VarietyType;

/// \class Ast
/// \brief The root of the AST hierarchy.
///
///  The Ast class is the common root of all AST nodes in the system, but it is
///  perhaps better to think of the immediate sub-classes of Ast as being the
///  roots of forest of hierarchies.  The main components, currently, are rooted
///  by the Type and Decl nodes.  The former begins a hierarchy of types,
///  whereas the latter correspond to declarations and definitions.
///
class Ast {

public:
    /// \brief Codes which identify concrete members of the AST hierarchy.
    ///
    /// Each concrete sub-class has an code which labels its identity.  These
    /// codes are used, in the main, to implement the LLVM mechanisms for type
    /// identification and casting (llvm::isa, llvm::dyn_cast, etc).
    ///
    /// Codes which do not begin with the prefix AST_ are considered to be
    /// internal to this enumeration (e.g. FIRST_Decl, LAST_Decl, etc). These
    /// codes should not be used directly and are subject to change.  Instead,
    /// use the provided predicate methods such as Ast::denotesDecl,
    /// Ast::denotesType, etc.
    ///
    /// NOTE: When adding/removing a member of this enumeration, be sure to
    /// update Ast::kindStrings as well.
    enum AstKind {

        //
        // Decl nodes.  There are currently four sub-categories.
        //
        //    - Model decls which denotes signatures and domains.
        //
        //    - Type declarations (models, carriers, etc).
        //
        //    - Subroutine decls denoting functions and procedures.
        //
        //    - Value decls denoting elements of a domain.
        //
        AST_SignatureDecl,      ///< SignatureDecl
        AST_DomainDecl,         ///< DomainDecl
        AST_AbstractDomainDecl, ///< AbstractDomainDecl
        AST_DomainInstanceDecl, ///< DomainInstanceDecl
        AST_VarietyDecl,        ///< VarietyDecl
        AST_FunctorDecl,        ///< FunctorDecl
        AST_CarrierDecl,        ///< CarrierDecl
        AST_AddDecl,            ///< AddDecl
        AST_FunctionDecl,       ///< FunctionDecl
        AST_ProcedureDecl,      ///< ProcedureDecl
        AST_ParamValueDecl,     ///< ParamValueDecl
        AST_ObjectDecl,         ///< ObjectDecl

        //
        // Type nodes.
        //
        AST_SignatureType,      ///< SignatureType
        AST_VarietyType,        ///< VarietyType
        AST_FunctorType,        ///< FunctorType
        AST_DomainType,         ///< DomainType
        AST_FunctionType,       ///< FunctionType
        AST_ProcedureType,      ///< ProcedureType

        //
        // Expr nodes.
        //
        AST_DeclRefExpr,        ///< DeclRefExpr
        AST_KeywordSelector,    ///< KeywordSelector
        AST_FunctionCallExpr,   ///< FunctionCallExpr

        //
        // Stmt nodes.
        //
        AST_BlockStmt,          ///< BlockStmt
        AST_ProcedureCallStmt,  ///< ProcedureCallStmt

        //
        // Delimitiers providing classification of the above codes.
        //
        LAST_AstKind,

        FIRST_Decl      = AST_SignatureDecl,
        LAST_Decl       = AST_ObjectDecl,

        FIRST_ModelDecl = AST_SignatureDecl,
        LAST_ModelDecl  = AST_FunctorDecl,

        FIRST_TypeDecl  = AST_SignatureDecl,
        LAST_TypeDecl   = AST_CarrierDecl,

        FIRST_ValueDecl = AST_ParamValueDecl,
        LAST_ValueDecl  = AST_ObjectDecl,

        FIRST_Type      = AST_SignatureType,
        LAST_Type       = AST_ProcedureType,

        FIRST_ModelType = AST_SignatureType,
        LAST_ModelType  = AST_DomainType,

        FIRST_Expr      = AST_DeclRefExpr,
        LAST_Expr       = AST_FunctionCallExpr,

        FIRST_Stmt      = AST_BlockStmt,
        LAST_Stmt       = AST_ProcedureCallStmt
    };

    virtual ~Ast() { }

    /// \brief  Accesses the code identifying this node.
    AstKind getKind() const { return kind; }

    /// \brief Accesses the location of this node.
    ///
    /// If no location information is available, or if this node was created
    /// internally by the compiler, an invalid location object (queriable by
    /// Location::isValid()) is returned.
    virtual Location getLocation() const { return Location(); }

    /// \brief Returns true if this node is valid.
    ///
    /// The validity of a node does not have an inforced semantics.  The Ast
    /// classes themselves never consult the validity bit.  Nodes are marked
    /// invalid by a client of the Ast (for example, the type checker) and the
    /// meaning of such a mark is left for the client to decide.
    bool isValid() const { return validFlag == true; }

    /// \brief Marks this node as invalid.
    void markInvalid() { validFlag = false; }

    /// \brief Returns true if one may call std::delete on this node.
    ///
    /// Certain nodes are not deletable since they are allocated in special
    /// containers which perform memoization or manage memory in a special way.
    ///
    /// \note  This property is likely to disappear in the future.
    bool isDeletable() const { return deletable; }

    /// \brief Returns true if this node denotes a declaration.
    bool denotesDecl() const {
        return (FIRST_Decl <= kind && kind <= LAST_Decl);
    }

    /// \brief Returns true if this node denotes a Model.
    bool denotesModelDecl() const {
        return (FIRST_ModelDecl <= kind && kind <= LAST_ModelDecl);
    }

    /// \brief Returns true if this node denotes a type declaration.
    bool denotesTypeDecl() const {
        return (FIRST_TypeDecl <= kind && kind <= LAST_TypeDecl);
    }

    /// \brief Returns true if this node denotes a subroutine decl (i.e. either
    /// a procedure or function decl).
    bool denotesSubroutineDecl() const {
        return (kind == AST_FunctionDecl ||
                kind == AST_ProcedureDecl);
    }

    /// \brief Returns true if this node denotes a Value.
    bool denotesValueDecl() const {
        return (FIRST_ValueDecl <= kind && kind <= LAST_ValueDecl);
    }

    /// \brief Returns true if this node denotes a Type.
    bool denotesType() const {
        return (FIRST_Type <= kind && kind <= LAST_Type);
    }

    /// \brief Returns true if this node denotes a model type.
    bool denotesModelType() const {
        return (FIRST_ModelType <= kind && kind <= LAST_ModelType);
    }

    /// \brief Returns true if this node denotes a subroutine type (i.e. either
    /// a procedure of function type).
    bool denotesSubroutineType() const {
        return (kind == AST_FunctionType ||
                kind == AST_ProcedureType);
    }

    /// \brief Returns true if this node denotes a expression.
    bool denotesExpr() const {
        return (FIRST_Expr <= kind && kind <= LAST_Expr);
    }

    /// \brief Returns true if this node denotes a Stmt.
    bool denotesStmt() const {
        return (FIRST_Stmt <= kind && kind <= LAST_Stmt);
    }

    /// \breif Returns a string matching the kind of this node.
    const char *getKindString() const { return kindStrings[kind]; }

    /// \brief Prints a representation of this ast node to stderr.
    virtual void dump();

    /// \brief Support isa and dyn_cast.
    static bool classof(const Ast *node) { return true; }

protected:
    /// \brief Initializes an Ast node of the specified kind.
    ///
    /// \param  kind  The kind of node to create.
    Ast(AstKind kind)
        : kind(kind),
          validFlag(true),
          deletable(true) { }

    AstKind  kind      : 8;     ///< The kind of this node.
    bool     validFlag : 1;     ///< True if this node is valid.
    bool     deletable : 1;     ///< True if we may call delete on this node.
    unsigned bits      : 23;    ///< Unused bits available to sub-classes.

    static const char *kindStrings[LAST_AstKind];
};

//===----------------------------------------------------------------------===//
// DeclarativeRegion
class DeclarativeRegion {

protected:
    DeclarativeRegion(Ast::AstKind kind)
        : declKind(kind), parent(0) { }

    DeclarativeRegion(Ast::AstKind kind, DeclarativeRegion *parent)
        : declKind(kind), parent(parent) { }

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

    // Adds the given decl to this region.  If a decl with the same name already
    // exists in this region that previous decl is unconditionally overwritten.
    // It is the responsibility of the caller to ensure that this operation is
    // semantically valid.
    void addDecl(Decl *decl);

    // Adds the given declaration to the region using the supplied rewrite
    // rules.
    void addDeclarationUsingRewrites(const AstRewriter &rewrites,
                                     Decl *decl);

    // Adds the declarations from the given region to this one using the
    // supplied rewrite rules.
    void addDeclarationsUsingRewrites(const AstRewriter       &rewrites,
                                      const DeclarativeRegion *region);

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

    // Converts this DeclarativeRegion into a Decl node.
    Decl *asDecl();
    const Decl *asDecl() const;

    void addObserver(DeclarativeRegion *region) { observers.push_front(region); }

    static bool classof(const Ast *node) {
        switch (node->getKind()) {
        default:
            return false;
        case Ast::AST_DomainDecl:
        case Ast::AST_SignatureDecl:
        case Ast::AST_VarietyDecl:
        case Ast::AST_FunctorDecl:
        case Ast::AST_DomainInstanceDecl:
        case Ast::AST_AbstractDomainDecl:
        case Ast::AST_AddDecl:
            return true;
        }
    }

    static bool classof(const DomainDecl    *node) { return true; }
    static bool classof(const SignatureDecl *node) { return true; }
    static bool classof(const VarietyDecl   *node) { return true; }
    static bool classof(const FunctorDecl   *node) { return true; }
    static bool classof(const AddDecl       *node) { return true; }
    static bool classof(const DomainInstanceDecl *node) { return true; }
    static bool classof(const AbstractDomainDecl *node) { return true; }

protected:
    virtual void notifyAddDecl(Decl *decl);
    virtual void notifyRemoveDecl(Decl *decl);

private:
    Ast::AstKind       declKind;
    DeclarativeRegion *parent;

    typedef std::list<DeclarativeRegion*> ObserverList;
    ObserverList observers;

    void notifyObserversOfAddition(Decl *decl);
    void notifyObserversOfRemoval(Decl *decl);
};

} // End comma namespace.

namespace llvm {

// Specialize isa_impl_wrap to test if a DeclarativeRegion is a specific Decl.
template<class To>
struct isa_impl_wrap<To,
                     const comma::DeclarativeRegion, const comma::DeclarativeRegion> {
    static bool doit(const comma::DeclarativeRegion &val) {
        return To::classof(val.asDecl());
    }
};

template<class To>
struct isa_impl_wrap<To, comma::DeclarativeRegion, comma::DeclarativeRegion>
  : public isa_impl_wrap<To,
                         const comma::DeclarativeRegion,
                         const comma::DeclarativeRegion> { };

// Decl to DeclarativeRegion conversions.
template<class From>
struct cast_convert_val<comma::DeclarativeRegion, From, From> {
    static comma::DeclarativeRegion &doit(const From &val) {
        return *val.asDeclarativeRegion();
    }
};

template<class From>
struct cast_convert_val<comma::DeclarativeRegion, From*, From*> {
    static comma::DeclarativeRegion *doit(const From *val) {
        return val->asDeclarativeRegion();
    }
};

template<class From>
struct cast_convert_val<const comma::DeclarativeRegion, From, From> {
    static const comma::DeclarativeRegion &doit(const From &val) {
        return *val.asDeclarativeRegion();
    }
};

template<class From>
struct cast_convert_val<const comma::DeclarativeRegion, From*, From*> {
    static const comma::DeclarativeRegion *doit(const From *val) {
        return val->asDeclarativeRegion();
    }
};

// DeclarativeRegion to Decl conversions.
template<class To>
struct cast_convert_val<To,
                        const comma::DeclarativeRegion,
                        const comma::DeclarativeRegion> {
    static To &doit(const comma::DeclarativeRegion &val) {
        return *reinterpret_cast<To*>(
            const_cast<comma::Decl*>(val.asDecl()));
    }
};

template<class To>
struct cast_convert_val<To, comma::DeclarativeRegion, comma::DeclarativeRegion>
    : public cast_convert_val<To,
                              const comma::DeclarativeRegion,
                              const comma::DeclarativeRegion> { };

template<class To>
struct cast_convert_val<To,
                        const comma::DeclarativeRegion*,
                        const comma::DeclarativeRegion*> {
    static To *doit(const comma::DeclarativeRegion *val) {
        return reinterpret_cast<To*>(
            const_cast<comma::Decl*>(val->asDecl()));
    }
};

template<class To>
struct cast_convert_val<To, comma::DeclarativeRegion*, comma::DeclarativeRegion*>
    : public cast_convert_val<To,
                              const comma::DeclarativeRegion*,
                              const comma::DeclarativeRegion*> { };

} // End llvm namespace.

#endif
