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

//
//  The Ast class is the root of the ast hierarchy.
//
class Ast {

public:
    enum AstKind {

        //
        // Decls
        //
        AST_SignatureDecl,
        AST_DomainDecl,
        AST_VarietyDecl,
        AST_FunctorDecl,
        AST_FunctionDecl,

        //
        // Types
        //
        AST_SignatureType,
        AST_VarietyType,
        AST_FunctorType,
        AST_DomainType,
        AST_ConcreteDomainType,
        AST_AbstractDomainType,
        AST_PercentType,
        AST_FunctionType,

        //
        // Delimiters classifying the above tags.
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

    // Returns the kind of this node.
    AstKind getKind() const { return kind; }

    // Returns a location object for this node.  If no location information is
    // available, or if this node was created internally by the compiler, a
    // location object for which isValid() is true is returned.
    virtual Location getLocation() const { return Location(); }

    // Returns true if this node is valid.
    bool isValid() const { return validFlag == true; }

    // Marks this node as invalid.
    void markInvalid() { validFlag = false; }

    // Returns true if one may call "delete" on this node.  Certain nodes are
    // not deletable since they are allocated in special containers which
    // perform memoization or manage memory in a special way.  Always test if a
    // node is deletable.
    bool isDeletable() const { return deletable; }

    bool denotesDecl() const {
        return (FIRST_Decl <= this->getKind() &&
                this->getKind() <= LAST_Decl);
    }

    bool denotesTypeDecl() const {
        return (FIRST_TypeDecl <= this->getKind() &&
                this->getKind() <= LAST_TypeDecl);
    }

    bool denotesModelDecl() const {
        return (FIRST_ModelDecl <= this->getKind() &&
                this->getKind() <= LAST_ModelDecl);
    }

    bool denotesType() const {
        return (FIRST_Type <= this->getKind() &&
                this->getKind() <= LAST_Type);
    }

    bool denotesDomainType() const {
        return (kind == AST_ConcreteDomainType ||
                kind == AST_AbstractDomainType ||
                kind == AST_PercentType);
    }

    bool denotesModelType() const {
        return (denotesDomainType()       ||
                kind == AST_SignatureType ||
                kind == AST_VarietyType   ||
                kind == AST_FunctorType);
    }

    // Support isa and dyn_cast.
    static bool classof(const Ast *node) { return true; }

protected:
    Ast(AstKind kind)
        : kind(kind),
          validFlag(true),
          deletable(true) { }

    AstKind  kind      : 8;
    bool     validFlag : 1;     // Is this node valid?
    bool     deletable : 1;     // Can we call delete on this node?
    unsigned bits      : 23;
};

} // End comma namespace.

#endif
