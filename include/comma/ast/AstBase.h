//===-- ast/AstBase.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
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
class Ast;
class CompilationUnit;
class Decl;
class DomainDecl;
class DomainType;
class Domoid;
class FunctorDecl;
class ModelDecl;
class ModelType;
class NamedDecl;
class PercentType;
class ParameterizedModel;
class Sigoid;
class SignatureDecl;
class SignatureType;
class Type;
class TypeDecl;
class VarietyDecl;

//
//  The ast class is the root of the ast hierarchy.
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

        //
        // Types
        //
        AST_SignatureType,
        AST_DomainType,
        AST_PercentType,
        AST_FunctionType,

        //
        // Delimiters classifying the above tags.
        //
        FIRST_Decl      = AST_SignatureDecl,
        LAST_Decl       = AST_FunctorDecl,

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
        return kind == AST_DomainType || kind == AST_PercentType;
    }

    bool denotesModelType() const {
        return denotesDomainType() || kind == AST_SignatureType;
    }

    // Support isa and dyn_cast.
    static bool classof(const Ast *node) { return true; }

protected:
    Ast(AstKind kind) : kind(kind), validFlag(true) { }

    AstKind  kind      : 8;
    bool     validFlag : 1;
    unsigned bits      : 23;
};

} // End comma namespace.

#endif
