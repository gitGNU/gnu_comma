//===-- ast/AttribExpr.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief AST node definitions for attributes which resolve to expressions.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ATTRIBEXPR_HDR_GUARD
#define COMMA_AST_ATTRIBEXPR_HDR_GUARD

#include "comma/ast/Expr.h"
#include "comma/basic/Attributes.h"

namespace comma {

//===----------------------------------------------------------------------===//
// AttribExpr
//
/// The AttribExpr class represents attributes which resolve to values.
class AttribExpr : public Expr {

public:
    /// \brief Returns the prefix to which this attribute applies.
    ///
    /// The returned node is either a TypeRef, SubroutineRef, or an Expr.
    const Ast *getPrefix() const { return prefix; }
    Ast *getPrefix() { return prefix; }

    /// Returns the AttributeID associated with this expression.
    attrib::AttributeID getAttributeID() const {
        // AttributeID's are stored in the Ast bits field.
        return static_cast<attrib::AttributeID>(bits);
    }

    // Support isa and dyn_cast.
    static bool classof(const AttribExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesAttribExpr();
    }

protected:
    /// Constructs an AttribExpr when the type of the attribute is known.
    AttribExpr(AstKind kind, Ast *prefix, Type *type, Location loc)
        : Expr(kind, type, loc),
          prefix(prefix) {
        bits = correspondingID(kind);
        assert(this->denotesAttribExpr());
        assert(getAttributeID() != attrib::UNKNOWN_ATTRIBUTE);
    }

    /// Constructs an AttribExpr when the exact type is not available.
    /// Subclasses should call Expr::setType to resolve the type when available.
    AttribExpr(AstKind kind, Ast *prefix, Location loc)
        : Expr(kind, loc),
          prefix(prefix) {
        bits = correspondingID(kind);
        assert(this->denotesAttribExpr());
        assert(getAttributeID() != attrib::UNKNOWN_ATTRIBUTE);
    }

    /// Returns the AttributeID which corresponds to the given AstKind, or
    /// attrib::UNKNOWN_ATTRIB if there is no mapping.
    static attrib::AttributeID correspondingID(AstKind kind);

    Ast *prefix;
};

//===----------------------------------------------------------------------===//
// FirstAE
//
/// Attribute representing <tt>S'First</tt>, where \c S is a scalar subtype.
/// This attribute denotes the lower bound of \c S, represented as an element of
/// the type of S.  Note that this attribute is not necessarily static, as
/// scalar subtypes can have dynamic bounds.
class FirstAE : public AttribExpr {

public:
    FirstAE(IntegerSubType *prefix, Location loc)
        : AttribExpr(AST_FirstAE, prefix, prefix, loc) { }

    //@{
    /// Specializations of AttribExpr::getPrefix().
    const IntegerSubType *getPrefix() const {
        return llvm::cast<IntegerSubType>(prefix);
    }

    IntegerSubType *getPrefix() {
        return llvm::cast<IntegerSubType>(prefix);
    }
    //@}


    //@{
    /// Specializations of Expr::getType().
    const IntegerSubType *getType() const {
        return llvm::cast<IntegerSubType>(Expr::getType());
    }

    IntegerSubType *getType() {
        return llvm::cast<IntegerSubType>(Expr::getType());
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const FirstAE *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FirstAE;
    }
};

//===----------------------------------------------------------------------===//
// LastAE
//
/// Attribute representing <tt>S'First</tt>, where \c S is a scalar subtype.
/// This attribute denotes the upper bound of \c S, represented as an element of
/// the type of S.  Note that this attribute is not necessarily static, as
/// scalar subtypes can have dynamic bounds.
class LastAE : public AttribExpr {

public:
    LastAE(IntegerSubType *prefix, Location loc)
        : AttribExpr(AST_LastAE, prefix, prefix, loc) { }

    //@{
    /// Specializations of AttribExpr::getPrefix().
    const IntegerSubType *getPrefix() const {
        return llvm::cast<IntegerSubType>(prefix);
    }

    IntegerSubType *getPrefix() {
        return llvm::cast<IntegerSubType>(prefix);
    }
    //@}

    //@{
    /// Specializations of Expr::getType().
    const IntegerSubType *getType() const {
        return llvm::cast<IntegerSubType>(Expr::getType());
    }

    IntegerSubType *getType() {
        return llvm::cast<IntegerSubType>(Expr::getType());
    }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const LastAE *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_LastAE;
    }
};

} // end comma namespace.

#endif
