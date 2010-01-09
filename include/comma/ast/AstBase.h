//===-- ast/AstBase.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
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
class AggregateExpr;
class ArrayBoundAE;
class ArrayDecl;
class ArrayRangeAttrib;
class ArraySubtypeDecl;
class ArrayType;
class AssignmentStmt;
class Ast;
class AstRewriter;
class AstResource;
class AttribExpr;
class BlockStmt;
class CarrierDecl;
class ComponentDecl;
class CompositeType;
class CompilationUnit;
class Decl;
class DeclRegion;
class DeclRefExpr;
class DeclRewriter;
class DiscreteType;
class DomainDecl;
class DomainInstanceDecl;
class DomainType;
class DomainTypeDecl;
class Domoid;
class DSTDefinition;
class EnumerationDecl;
class EnumLiteral;
class EnumerationType;
class EnumSubtypeDecl;
class ExceptionDecl;
class ExceptionRef;
class Expr;
class FirstAE;
class FirstArrayAE;
class ForStmt;
class FunctionCallExpr;
class FunctionDecl;
class FunctionType;
class FunctorDecl;
class FunctorType;
class HandlerStmt;
class Identifier;
class IfStmt;
class ImportDecl;
class IndexedArrayExpr;
class IncompleteType;
class IncompleteTypeDecl;
class InjExpr;
class IntegerDecl;
class IntegerLiteral;
class IntegerSubtypeDecl;
class IntegerType;
class KeywordSelector;
class LastAE;
class LastArrayAE;
class LoopDecl;
class LoopStmt;
class ModelDecl;
class ObjectDecl;
class ParamValueDecl;
class PercentDecl;
class Pragma;
class PragmaAssert;
class PragmaImport;
class PragmaStmt;
class PrimaryType;
class PrjExpr;
class ProcedureCallStmt;
class ProcedureDecl;
class ProcedureType;
class RaiseStmt;
class Range;
class RangeAttrib;
class RecordDecl;
class RecordType;
class RenamedObjectDecl;
class ReturnStmt;
class ScalarBoundAE;
class ScalarRangeAttrib;
class SelectedExpr;
class Sigoid;
class SignatureDecl;
class SigInstanceDecl;
class Stmt;
class StmtSequence;
class StringLiteral;
class SubroutineCall;
class SubroutineDecl;
class SubroutineRef;
class SubroutineType;
class SubtypeDecl;
class Type;
class ConversionExpr;
class TypeDecl;
class TypeRef;
class ValueDecl;
class VarietyDecl;
class VarietyType;
class WhileStmt;

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
        //    - Type declarations (carriers, enums, etc).
        //
        //    - Subroutine decls denoting functions and procedures.
        //
        //    - Value decls denoting elements of a type.
        //
        AST_SignatureDecl,      ///< SignatureDecl
        AST_DomainDecl,         ///< DomainDecl
        AST_VarietyDecl,        ///< VarietyDecl
        AST_FunctorDecl,        ///< FunctorDecl
        AST_AddDecl,            ///< AddDecl

        AST_CarrierDecl,        ///< CarrierDecl
        AST_EnumerationDecl,    ///< EnumerationDecl
        AST_IncompleteTypeDecl, ///< IncompleteTypeDecl
        AST_IntegerDecl,        ///< IntegerDecl
        AST_ArrayDecl,          ///< ArrayDecl
        AST_RecordDecl,         ///< RecordDecl
        AST_AbstractDomainDecl, ///< AbstractDomainDecl
        AST_DomainInstanceDecl, ///< DomainInstanceDecl
        AST_PercentDecl,        ///< PercentDecl

        AST_ArraySubtypeDecl,   ///< ArraySubtypeDecl
        AST_EnumSubtypeDecl,    ///< EnumSubtypeDecl
        AST_IntegerSubtypeDecl, ///< IntegerSubtypeDecl

        AST_SigInstanceDecl,    ///< SigInstanceDecl

        //
        // Value declaration nodes.
        //
        AST_LoopDecl,           ///< LoopDecl
        AST_ObjectDecl,         ///< ObjectDecl
        AST_ParamValueDecl,     ///< ParamValueDecl
        AST_RenamedObjectDecl,  ///< RenamedObjectDecl

        AST_FunctionDecl,       ///< FunctionDecl
        AST_ProcedureDecl,      ///< ProcedureDecl
        AST_EnumLiteral,        ///< EnumLiteral
        AST_ImportDecl,         ///< ImportDecl
        AST_ExceptionDecl,      ///< ExceptionDecl
        AST_ComponentDecl,      ///< ComponentDecl

        //
        // Type nodes.
        //
        AST_FunctionType,       ///< FunctionType
        AST_ProcedureType,      ///< ProcedureType

        //
        // Primary Types.
        //
        AST_ArrayType,          ///< ArrayType
        AST_DomainType,         ///< DomainType
        AST_EnumerationType,    ///< EnumerationType
        AST_IncompleteType,     ///< IncompleteType
        AST_IntegerType,        ///< IntegerType
        AST_RecordType,         ///< RecordType

        //
        // Expr nodes.
        //
        AST_ConversionExpr,     ///< ConversionExpr
        AST_DeclRefExpr,        ///< DeclRefExpr
        AST_FunctionCallExpr,   ///< FunctionCallExpr
        AST_IndexedArrayExpr,   ///< IndexedArrayExpr
        AST_InjExpr,            ///< InjExpr
        AST_IntegerLiteral,     ///< IntegerLiteral
        AST_AggregateExpr,      ///< AggregateExpr
        AST_PrjExpr,            ///< PrjExpr
        AST_SelectedExpr,       ///< SelectedExpr
        AST_StringLiteral,      ///< StringLiteral

        // Expr attributes.
        AST_FirstAE,            ///< FirstAE
        AST_FirstArrayAE,       ///< FirstArrayAE
        AST_LastArrayAE,        ///< LastArrayAE
        AST_LastAE,             ///< LastAE

        //
        // Stmt nodes.
        //
        AST_AssignmentStmt,     ///< AssignmentStmt
        AST_BlockStmt,          ///< BlockStmt
        AST_ForStmt,            ///< ForStmt
        AST_HandlerStmt,        ///< HandlerStmt
        AST_IfStmt,             ///< IfStmt
        AST_LoopStmt,           ///< LoopStmt
        AST_ProcedureCallStmt,  ///< ProcedureCallStmt
        AST_RaiseStmt,          ///< RaiseStmt
        AST_ReturnStmt,         ///< ReturnStmt
        AST_StmtSequence,       ///< StmtSequence
        AST_WhileStmt,          ///< WhileStmt
        AST_PragmaStmt,         ///< PragmaStmt

        //
        // Miscellaneous helper nodes.
        //
        AST_KeywordSelector,    ///< KeywordSelector
        AST_DSTDefinition,      ///< DSTDefinition
        AST_Range,              ///< Range
        AST_ArrayRangeAttrib,   ///< ArrayRangeAttrib
        AST_ScalarRangeAttrib,  ///< ScalarRangeAttrib
        AST_SubroutineRef,      ///< SubroutineRef
        AST_TypeRef,            ///< TypeRef
        AST_ExceptionRef,       ///< ExceptionRef
        AST_Identifier,         ///< Identifier
        AST_ComponentKey,       ///< ComponentKey

        //
        // Delimitiers providing classification of the above codes.
        //
        LAST_AstKind,

        FIRST_Decl = AST_SignatureDecl,
        LAST_Decl = AST_ComponentDecl,

        FIRST_ModelDecl = AST_SignatureDecl,
        LAST_ModelDecl = AST_FunctorDecl,

        FIRST_TypeDecl = AST_CarrierDecl,
        LAST_TypeDecl = AST_IntegerSubtypeDecl,

        FIRST_SubtypeDecl = AST_ArraySubtypeDecl,
        LAST_SubtypeDecl = AST_IntegerSubtypeDecl,

        FIRST_DomainType = AST_AbstractDomainDecl,
        LAST_DomainType = AST_PercentDecl,

        FIRST_ValueDecl = AST_LoopDecl,
        LAST_ValueDecl = AST_RenamedObjectDecl,

        FIRST_Type = AST_FunctionType,
        LAST_Type = AST_RecordType,

        FIRST_PrimaryType = AST_ArrayType,
        LAST_PrimaryType = AST_RecordType,

        FIRST_Expr = AST_ConversionExpr,
        LAST_Expr = AST_LastAE,

        FIRST_AttribExpr = AST_FirstAE,
        LAST_AttribExpr = AST_LastAE,

        FIRST_Stmt = AST_AssignmentStmt,
        LAST_Stmt = AST_PragmaStmt
    };

    virtual ~Ast() { }

    ///  Accesses the code identifying this node.
    AstKind getKind() const { return kind; }

    /// Accesses the location of this node.
    ///
    /// If no location information is available, or if this node was created
    /// internally by the compiler, an invalid location object (queriable by
    /// Location::isValid()) is returned.
    virtual Location getLocation() const { return Location(); }

    /// Returns true if this node is valid.
    ///
    /// The validity of a node does not have an inforced semantics.  The Ast
    /// classes themselves never consult the validity bit.  Nodes are marked
    /// invalid by a client of the Ast (for example, the type checker) and the
    /// meaning of such a mark is left for the client to decide.
    bool isValid() const { return validFlag == true; }

    /// Marks this node as invalid.
    void markInvalid() { validFlag = false; }

    /// Returns true if one may call std::delete on this node.
    ///
    /// Certain nodes are not deletable since they are allocated in special
    /// containers which perform memoization or manage memory in a special way.
    ///
    /// \note  This property is likely to disappear in the future.
    bool isDeletable() const { return deletable; }

    /// Returns true if this node denotes a declaration.
    bool denotesDecl() const {
        return (FIRST_Decl <= kind && kind <= LAST_Decl);
    }

    /// Returns true if this node denotes a Model.
    bool denotesModelDecl() const {
        return (FIRST_ModelDecl <= kind && kind <= LAST_ModelDecl);
    }

    /// Returns true if this node denotes a type declaration.
    bool denotesTypeDecl() const {
        return (FIRST_TypeDecl <= kind && kind <= LAST_TypeDecl);
    }

    /// Returns true if this node denotes a subtype declaration.
    bool denotesSubtypeDecl() const {
        return (FIRST_SubtypeDecl <= kind && kind <= LAST_SubtypeDecl);
    }

    /// Returns true if this node denotes a subroutine decl (i.e. either
    /// a procedure or function decl).
    bool denotesSubroutineDecl() const {
        return (kind == AST_FunctionDecl ||
                kind == AST_ProcedureDecl ||
                kind == AST_EnumLiteral);
    }

    /// Returns true if this node denotes a Value.
    bool denotesValueDecl() const {
        return (FIRST_ValueDecl <= kind && kind <= LAST_ValueDecl);
    }

    /// Returns true if this node denotes a domain type decl.
    bool denotesDomainTypeDecl() const {
        return (FIRST_DomainType <= kind && kind <= LAST_DomainType);
    }

    /// Returns true if this node denotes a Type.
    bool denotesType() const {
        return (FIRST_Type <= kind && kind <= LAST_Type);
    }

    /// Returns true if this node denotes a PrimaryType.
    bool denotesPrimaryType() const {
        return (FIRST_PrimaryType <= kind && kind <= LAST_PrimaryType);
    }

    /// Returns true if this node denotes a subroutine type (i.e. either a
    /// procedure of function type).
    bool denotesSubroutineType() const {
        return (kind == AST_FunctionType ||
                kind == AST_ProcedureType);
    }

    /// Returns true if this node denotes a composite type.
    bool denotesCompositeType() const {
        return (kind == AST_ArrayType || kind == AST_RecordType);
    }

    /// Returns true if this node denotes a expression.
    bool denotesExpr() const {
        return (FIRST_Expr <= kind && kind <= LAST_Expr);
    }

    /// Returns true if this node denotes an AttribExpr.
    bool denotesAttribExpr() const {
        return (FIRST_AttribExpr <= kind && kind <= LAST_AttribExpr);
    }

    /// Returns true if this node denotes a Stmt.
    bool denotesStmt() const {
        return (FIRST_Stmt <= kind && kind <= LAST_Stmt);
    }

    /// Returns a string matching the kind of this node.
    const char *getKindString() const { return kindStrings[kind]; }

    /// Prints a representation of this ast node to stderr.
    virtual void dump();

    /// Support isa and dyn_cast.
    static bool classof(const Ast *node) { return true; }

protected:
    /// Initializes an Ast node of the specified kind.
    ///
    /// \param  kind  The kind of node to create.
    Ast(AstKind kind)
        : kind(kind),
          validFlag(true),
          deletable(true),
          bits(0) { }

    AstKind  kind      : 8;     ///< The kind of this node.
    bool     validFlag : 1;     ///< True if this node is valid.
    bool     deletable : 1;     ///< True if we may call delete on this node.
    unsigned bits      : 23;    ///< Unused bits available to sub-classes.

    static const char *kindStrings[LAST_AstKind];
};

} // End comma namespace.

#endif
