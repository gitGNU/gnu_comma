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

    /// Returns the associated AttributeID.
    attrib::AttributeID getAttributeID() const {
        return id;
    }

    // Support isa and dyn_cast.
    static bool classof(const AttribExpr *node) { return true; }
    static bool classof(const Ast *node) {
        return node->denotesAttribExpr();
    }

protected:
    /// Constructs an AttribExpr when the type of the attribute is known.
    AttribExpr(AstKind kind,
               Ast *prefix, attrib::AttributeID id, Type *type, Location loc)
        : Expr(kind, type, loc), prefix(prefix), id(id) {
        assert(this->denotesAttribExpr());
        assert(id != attrib::UNKNOWN_ATTRIBUTE);
    }

    /// Constructs an AttribExpr when the exact type is not available.
    /// Subclasses should call Expr::setType to resolve the type when available.
    AttribExpr(AstKind kind,
               Ast *prefix, attrib::AttributeID id, Location loc)
        : Expr(kind, loc), prefix(prefix), id(id) {
        assert(this->denotesAttribExpr());
        assert(id != attrib::UNKNOWN_ATTRIBUTE);
    }

    Ast *prefix;
    attrib::AttributeID id;
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
        : AttribExpr(AST_FirstAE, prefix, attrib::First, prefix, loc) { }

    //@{
    /// Specializations of AttribExpr::getPrefix().
    const IntegerSubType *getPrefix() const {
        return llvm::cast<IntegerSubType>(prefix);
    }

    IntegerSubType *getPrefix() {
        return llvm::cast<IntegerSubType>(prefix);
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
        : AttribExpr(AST_LastAE, prefix, attrib::Last, prefix, loc) { }

    //@{
    /// Specializations of AttribExpr::getPrefix().
    const IntegerSubType *getPrefix() const {
        return llvm::cast<IntegerSubType>(prefix);
    }

    IntegerSubType *getPrefix() {
        return llvm::cast<IntegerSubType>(prefix);
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
