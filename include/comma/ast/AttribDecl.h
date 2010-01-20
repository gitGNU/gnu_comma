//===-- ast/AttribDecl.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ATTRIBDECL_HDR_GUARD
#define COMMA_AST_ATTRIBDECL_HDR_GUARD

#include "comma/ast/Decl.h"

namespace comma {

//===----------------------------------------------------------------------===//
// FunctionAttribDecl
//
/// This simple class provides a common root for all attributes which denote
/// functions.
class FunctionAttribDecl : public FunctionDecl {

public:
    /// Returns the AttributeID associated with this expression.
    attrib::AttributeID getAttributeID() const {
        // AttributeID's are stored in the lower Ast bits field.
        return static_cast<attrib::AttributeID>(bits);
    }

    //@{
    /// Returns the prefix associated with this function attribute.
    ///
    /// All function attribute declarations have primary types as prefix.
    const PrimaryType *getPrefix() const { return prefix; }
    PrimaryType *getPrefix() { return prefix; }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const FunctionAttribDecl *node) { return true; }
    static bool classof(const Ast *node) {
        return denotesFunctionAttribDecl(node);
    }

protected:
    FunctionAttribDecl(AstKind kind, PrimaryType *prefix,
                       IdentifierInfo *name, Location loc,
                       IdentifierInfo **keywords, FunctionType *type,
                       DeclRegion *parent)
        : FunctionDecl(kind, name, loc, keywords, type, parent),
          prefix(prefix) {
        bits = correspondingID(kind);
    }

private:
    static bool denotesFunctionAttribDecl(const Ast *node) {
        AstKind kind = node->getKind();
        return kind == AST_PosAD || kind == AST_ValAD;
    }

    static attrib::AttributeID correspondingID(AstKind kind) {
        attrib::AttributeID ID;
        switch (kind) {
        default:
            assert(false && "Bad ast kind for this attribute!");
            ID = attrib::UNKNOWN_ATTRIBUTE;
            break;
        case AST_PosAD:
            ID = attrib::Pos;
        case AST_ValAD:
            ID = attrib::Val;
        }
        return ID;
    }

    PrimaryType *prefix;
};

//===----------------------------------------------------------------------===//
// PosAD
//
/// This node represents the Pos attribute as applied to a discrete subtype
/// prefix.
class PosAD : public FunctionAttribDecl {

public:
    //@{
    /// Specialize FunctionAttribDecl::getPrefix.
    const DiscreteType *getPrefix() const {
        return llvm::cast<DiscreteType>(FunctionAttribDecl::getPrefix());
    }
    DiscreteType *getPrefix() {
        return llvm::cast<DiscreteType>(FunctionAttribDecl::getPrefix());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const PosAD *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_PosAD;
    }

private:
    /// The following static constructors are for use by IntegerDecl and
    /// EnumerationDecl.
    static PosAD *create(AstResource &resource, IntegerDecl *prefixDecl);
    static PosAD *create(AstResource &resource, EnumerationDecl *prefixDecl);

    friend class IntegerDecl;
    friend class EnumerationDecl;

    /// Private constructor for use by the static constructor functions.
    PosAD(DiscreteType *prefix, IdentifierInfo *name, Location loc,
          IdentifierInfo **keywords, FunctionType *type,
          DeclRegion *parent)
        : FunctionAttribDecl(AST_PosAD, prefix, name, loc,
                             keywords, type, parent) { }
};

//===----------------------------------------------------------------------===//
// ValAD
//
/// This node represents the Val attribute as applied to a discrete subtype
/// prefix.
class ValAD : public FunctionAttribDecl {

public:
    //@{
    /// Specialize FunctionAttribDecl::getPrefix.
    const DiscreteType *getPrefix() const {
        return llvm::cast<DiscreteType>(FunctionAttribDecl::getPrefix());
    }
    DiscreteType *getPrefix() {
        return llvm::cast<DiscreteType>(FunctionAttribDecl::getPrefix());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ValAD *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ValAD;
    }

private:
    /// The following static constructors are for use by IntegerDecl,
    /// IntegerSubtypeDecl, EnumerationDecl, and EnumSubtypeDecl.
    static ValAD *create(AstResource &resource, IntegerDecl *prefixDecl);
    static ValAD *create(AstResource &resource, EnumerationDecl *prefixDecl);

    friend class IntegerDecl;
    friend class EnumerationDecl;

    /// Private constructor for use by the static constructor functions.
    ValAD(DiscreteType *prefix, IdentifierInfo *name, Location loc,
          IdentifierInfo **keywords, FunctionType *type,
          DeclRegion *parent)
        : FunctionAttribDecl(AST_ValAD, prefix, name, loc,
                             keywords, type, parent) { }
};

} // end comma namespace.

#endif

