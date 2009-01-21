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
#include <iosfwd>

namespace comma {

//
// Forward declarations for all Ast nodes.
//
class AbstractDomainType;
class Ast;
class AstRewriter;
class CompilationUnit;
class ConcreteDomainType;
class Decl;
class DomainDecl;
class DomainType;
class Domoid;
class FunctionDecl;
class FunctionType;
class FunctorDecl;
class FunctorType;
class ModelDecl;
class ModelType;
class NamedDecl;
class PercentType;
class ParameterizedModel;
class ParameterizedType;
class Sigoid;
class SignatureDecl;
class SignatureType;
class Type;
class TypeDecl;
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
    enum AstKind {

        //
        // Decl nodes.
        //
        AST_SignatureDecl,      ///< SignatureDecl
        AST_DomainDecl,         ///< DomainDecl
        AST_VarietyDecl,        ///< VarietyDecl
        AST_FunctorDecl,        ///< FunctorDecl
        AST_FunctionDecl,       ///< FunctionDecl

        //
        // Type nodes.
        //
        AST_SignatureType,      ///< SignatureType
        AST_VarietyType,        ///< VarietyType
        AST_FunctorType,        ///< FunctorType
        AST_DomainType,         ///< DomainType
        AST_ConcreteDomainType, ///< ConcreteDomainType
        AST_AbstractDomainType, ///< AbstractDomainType
        AST_PercentType,        ///< PercentType
        AST_FunctionType,       ///< FunctionType

        //
        // Delimitiers providing classification of the above codes.
        //
        FIRST_Decl      = AST_SignatureDecl,
        LAST_Decl       = AST_FunctionDecl,

        FIRST_TypeDecl  = AST_SignatureDecl,
        LAST_TypeDecl   = AST_FunctorDecl,

        FIRST_ModelDecl = AST_SignatureDecl,
        LAST_ModelDecl  = AST_FunctorDecl,

        FIRST_Type      = AST_SignatureType,
        LAST_Type       = AST_FunctionType
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
        return (FIRST_Decl <= this->getKind() &&
                this->getKind() <= LAST_Decl);
    }

    /// \brief Returns true if this node denotes a declaration which also
    /// describes a type (i.e. SignatureDecl, DomainDecl, etc).
    bool denotesTypeDecl() const {
        return (FIRST_TypeDecl <= this->getKind() &&
                this->getKind() <= LAST_TypeDecl);
    }

    /// \brief Returns true if this node denotes a Model.
    bool denotesModelDecl() const {
        return (FIRST_ModelDecl <= this->getKind() &&
                this->getKind() <= LAST_ModelDecl);
    }

    /// \brief Returns true if this node denotes a Type.
    bool denotesType() const {
        return (FIRST_Type <= this->getKind() &&
                this->getKind() <= LAST_Type);
    }

    /// \brief Returns true if this node denotes a domain type.
    bool denotesDomainType() const {
        return (kind == AST_ConcreteDomainType ||
                kind == AST_AbstractDomainType ||
                kind == AST_PercentType);
    }

    /// \brief Returns true if this node denotes a model type.
    bool denotesModelType() const {
        return (denotesDomainType()       ||
                kind == AST_SignatureType ||
                kind == AST_VarietyType   ||
                kind == AST_FunctorType);
    }

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
    unsigned bits      : 23;    ///< Unused bits.
};

} // End comma namespace.

#endif
