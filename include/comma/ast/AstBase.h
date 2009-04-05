//===-- ast/AstBase.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
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
#include <iosfwd>

namespace comma {

//
// Forward declarations for all Ast nodes.
//
class AbstractDomainDecl;
class AddDecl;
class AssignmentStmt;
class Ast;
class AstRewriter;
class BlockStmt;
class CarrierDecl;
class CarrierType;
class CompilationUnit;
class Decl;
class DeclarativeRegion;
class DeclRefExpr;
class DomainDecl;
class DomainInstanceDecl;
class DomainType;
class Domoid;
class EnumerationDecl;
class EnumerationLiteral;
class EnumerationType;
class Expr;
class FunctionCallExpr;
class FunctionDecl;
class FunctionType;
class FunctorDecl;
class FunctorType;
class IfStmt;
class ImportDecl;
class InjExpr;
class KeywordSelector;
class ModelDecl;
class ModelType;
class ObjectDecl;
class ParameterizedModel;
class ParameterizedType;
class ParamValueDecl;
class PrjExpr;
class ProcedureCallStmt;
class ProcedureDecl;
class ProcedureType;
class Qualifier;
class ReturnStmt;
class Sigoid;
class SignatureDecl;
class SignatureType;
class Stmt;
class StmtSequence;
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
        //    - Value decls denoting elements of a domain or type.
        //
        AST_SignatureDecl,      ///< SignatureDecl
        AST_DomainDecl,         ///< DomainDecl
        AST_AbstractDomainDecl, ///< AbstractDomainDecl
        AST_DomainInstanceDecl, ///< DomainInstanceDecl
        AST_VarietyDecl,        ///< VarietyDecl
        AST_FunctorDecl,        ///< FunctorDecl
        AST_CarrierDecl,        ///< CarrierDecl
        AST_EnumerationDecl,    ///< EnumerationDecl
        AST_AddDecl,            ///< AddDecl
        AST_FunctionDecl,       ///< FunctionDecl
        AST_ProcedureDecl,      ///< ProcedureDecl
        AST_ParamValueDecl,     ///< ParamValueDecl
        AST_EnumerationLiteral, ///< EnumerationLiteral
        AST_ObjectDecl,         ///< ObjectDecl
        AST_ImportDecl,         ///< ImportDecl

        //
        // Type nodes.
        //
        AST_SignatureType,      ///< SignatureType
        AST_VarietyType,        ///< VarietyType
        AST_FunctorType,        ///< FunctorType
        AST_DomainType,         ///< DomainType
        AST_CarrierType,        ///< CarrierType
        AST_FunctionType,       ///< FunctionType
        AST_ProcedureType,      ///< ProcedureType
        AST_EnumerationType,    ///< EnumerationType

        //
        // Expr nodes.
        //
        AST_DeclRefExpr,        ///< DeclRefExpr
        AST_FunctionCallExpr,   ///< FunctionCallExpr
        AST_InjExpr,            ///< InjExpr
        AST_KeywordSelector,    ///< KeywordSelector
        AST_PrjExpr,            ///< PrjExpr

        //
        // Stmt nodes.
        //
        AST_AssignmentStmt,     ///< AssignmentStmt
        AST_BlockStmt,          ///< BlockStmt
        AST_IfStmt,             ///< IfStmt
        AST_ProcedureCallStmt,  ///< ProcedureCallStmt
        AST_ReturnStmt,         ///< ReturnStmt
        AST_StmtSequence,       ///< StmtSequence

        //
        // Miscellaneous helper nodes.
        //
        AST_Qualifier,          ///< Qualifier

        //
        // Delimitiers providing classification of the above codes.
        //
        LAST_AstKind,

        FIRST_Decl      = AST_SignatureDecl,
        LAST_Decl       = AST_ImportDecl,

        FIRST_ModelDecl = AST_SignatureDecl,
        LAST_ModelDecl  = AST_FunctorDecl,

        FIRST_TypeDecl  = AST_SignatureDecl,
        LAST_TypeDecl   = AST_EnumerationDecl,

        FIRST_ValueDecl = AST_ParamValueDecl,
        LAST_ValueDecl  = AST_ObjectDecl,

        FIRST_Type      = AST_SignatureType,
        LAST_Type       = AST_EnumerationType,

        FIRST_ModelType = AST_SignatureType,
        LAST_ModelType  = AST_DomainType,

        FIRST_Expr      = AST_DeclRefExpr,
        LAST_Expr       = AST_PrjExpr,

        FIRST_Stmt      = AST_AssignmentStmt,
        LAST_Stmt       = AST_StmtSequence
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

    /// \brief Returns a string matching the kind of this node.
    const char *getKindString() const { return kindStrings[kind]; }

    /// \brief Prints a representation of this ast node to stderr.
    virtual void dump(unsigned depth = 0);

    /// \brief Utility to print the given number of spaces to stderr.  To be
    /// used in implementations of dump.
    static void dumpSpaces(unsigned n);

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

} // End comma namespace.

#endif
