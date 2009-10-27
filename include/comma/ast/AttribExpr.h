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
        // AttributeID's are stored in the lower Ast bits field.
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
// ScalarBoundAE
//
/// Represents a \c First or 'c Last attribute when applied to a scalar subtype.
class ScalarBoundAE : public AttribExpr {

public:
    virtual ~ScalarBoundAE() { }

    /// Returns true if this is a \c First attribute.
    bool isFirst() const { return llvm::isa<FirstAE>(this); }

    /// Returns true if this is a \c Last attribute.
    bool isLast() const { return llvm::isa<LastAE>(this); }

    //@{
    /// Specializations of AttribExpr::getPrefix().
    const IntegerType *getPrefix() const {
        return llvm::cast<IntegerType>(prefix);
    }
    IntegerType *getPrefix() {
        return llvm::cast<IntegerType>(prefix);
    }
    //@}

    //@{
    /// Specializations of Expr::getType().
    const IntegerType *getType() const {
        return llvm::cast<IntegerType>(Expr::getType());
    }
    IntegerType *getType() {
        return llvm::cast<IntegerType>(Expr::getType());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ScalarBoundAE *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return (kind == AST_FirstAE || kind == AST_LastAE);
    }

protected:
    ScalarBoundAE(AstKind kind, IntegerType *prefix, Location loc)
        : AttribExpr(kind, prefix, prefix, loc) {
        assert(kind == AST_FirstAE || kind == AST_LastAE);
    }
};

//===----------------------------------------------------------------------===//
// FirstAE
//
/// Attribute representing <tt>S'First</tt>, where \c S is a scalar subtype.
/// This attribute denotes the lower bound of \c S, represented as an element of
/// the type of S.  Note that this attribute is not necessarily static, as
/// scalar subtypes can have dynamic bounds.
class FirstAE : public ScalarBoundAE {

public:
    FirstAE(IntegerType *prefix, Location loc)
        : ScalarBoundAE(AST_FirstAE, prefix, loc) { }

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
class LastAE : public ScalarBoundAE {

public:
    LastAE(IntegerType *prefix, Location loc)
        : ScalarBoundAE(AST_LastAE, prefix, loc) { }

    // Support isa and dyn_cast.
    static bool classof(const LastAE *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_LastAE;
    }
};

//===----------------------------------------------------------------------===//
// ArrayBoundAE
//
/// Common base class for the attributes \c First and \c Last when applied to a
/// prefix of array type.
class ArrayBoundAE : public AttribExpr {

public:
    /// Returns true if the dimension associated with this arrtibute is
    /// implicit.
    bool hasImplicitDimension() const { return dimExpr == 0; }

    //@{
    /// Returns the dimension expression associated with this attribute, or null
    /// if the dimension is implicit for this node.
    Expr *getDimensionExpr() { return dimExpr; }
    const Expr *getDimensionExpr() const { return dimExpr; }
    //@}

    /// Returns the zero based dimension associated with this attribute.
    unsigned getDimension() const { return dimValue; }

    /// Returns true if this is a \c First attribute.
    bool isFirst() const;

    /// Returns true if this is a \c Last attribute.
    bool isLast() const;

    //@{
    /// Specializations of AttribExpr::getPrefix().
    const Expr *getPrefix() const {
        return llvm::cast<Expr>(prefix);
    }
    Expr *getPrefix() {
        return llvm::cast<Expr>(prefix);
    }
    //@}

    //@{
    /// Returns the type of the prefix.
    const ArrayType *getPrefixType() const {
        return llvm::cast<ArrayType>(getPrefix()->getType());
    }
    ArrayType *getPrefixType() {
        return llvm::cast<ArrayType>(getPrefix()->getType());
    }

    //@{
    /// Specializations of Expr::getType().
    const IntegerType *getType() const {
        return llvm::cast<IntegerType>(Expr::getType());
    }
    IntegerType *getType() {
        return llvm::cast<IntegerType>(Expr::getType());
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ArrayBoundAE *node) { return true; }
    static bool classof(const Ast *node) {
        AstKind kind = node->getKind();
        return (kind == AST_FirstArrayAE || kind == AST_LastArrayAE);
    }

protected:
    /// Creates an array bound attribute expression with an explicit dimension.
    ///
    /// \param prefix An expression which must resolve to an array type.
    ///
    /// \param dimension A positive static integer expression which does not
    /// exceed the dimensionality of the type of \p prefix.
    ///
    /// \param loc the location of the attribute.
    ArrayBoundAE(AstKind kind, Expr *prefix, Expr *dimension, Location loc);

    /// Creates an array bound arribute expression with an implicit dimension of
    /// 1.
    ///
    /// \param prefix An expression which must resolve to an array type.
    ///
    /// \param loc the location of the attribute.
    ArrayBoundAE(AstKind kind, Expr *prefix, Location loc);

    /// The expression node defining the associated dimension, or null if the
    /// dimension is implicit.
    Expr *dimExpr;

    /// The static value of the dimension expression, or 0 if the dimension is
    /// implicit.
    unsigned dimValue;
};

//===----------------------------------------------------------------------===//
// FirstArrayAE
//
/// Represents the attribute <tt>A'First(N)</tt>, where \c A is an object of
/// array type and \c N is a positive static integer expression denoting the
/// dimension to which the attribute applies.  The dimension is not required to
/// be explicit, in which case the index defaults to 1.
class FirstArrayAE : public ArrayBoundAE {

public:
    /// Creates a \c First attribute expression with an explicit dimension.
    ///
    /// \param prefix An expression which must resolve to an array type.
    ///
    /// \param dimension A positive static integer expression which does not
    /// exceed the dimensionality of the type of \p prefix.
    ///
    /// \param loc the location of the attribute.
    FirstArrayAE(Expr *prefix, Expr *dimension, Location loc)
        : ArrayBoundAE(AST_FirstArrayAE, prefix, dimension, loc) { }

    /// Creates a \c First arribute expression with an implicit dimension of 1.
    ///
    /// \param prefix An expression which must resolve to an array type.
    ///
    /// \param loc the location of the attribute.
    FirstArrayAE(Expr *prefix, Location loc)
        : ArrayBoundAE(AST_FirstArrayAE, prefix, loc) { }

    // Support isa/dyn_cast.
    static bool classof(const FirstArrayAE *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_FirstArrayAE;
    }
};

//===----------------------------------------------------------------------===//
// LastArrayAE
//
/// Represents the attribute <tt>A'Last(N)</tt>, where \c A is an object of
/// array type and \c N is a positive static integer expression denoting the
/// dimension to which the attribute applies.  The dimension is not required to
/// be explicit, in which case the index defaults to 1.
class LastArrayAE : public ArrayBoundAE {

public:
    /// Creates a \c Last attribute expression with an explicit dimension.
    ///
    /// \param prefix An expression which must resolve to an array type.
    ///
    /// \param dimension A positive static integer expression which does not
    /// exceed the dimensionality of the type of \p prefix.
    ///
    /// \param loc the location of the attribute.
    LastArrayAE(Expr *prefix, Expr *dimension, Location loc)
        : ArrayBoundAE(AST_LastArrayAE, prefix, dimension, loc) { }

    /// Creates a \c Last arribute expression with an implicit dimension of 1.
    ///
    /// \param prefix An expression which must resolve to an array type.
    ///
    /// \param loc the location of the attribute.
    LastArrayAE(Expr *prefix, Location loc)
        : ArrayBoundAE(AST_LastArrayAE, prefix, loc) { }

    // Support isa/dyn_cast.
    static bool classof(const LastArrayAE *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_LastArrayAE;
    }
};

//===----------------------------------------------------------------------===//
// Inline methods now that the hierarchy is in place.

inline bool ArrayBoundAE::isFirst() const {
    return llvm::isa<FirstArrayAE>(this);
}

inline bool ArrayBoundAE::isLast() const {
    return llvm::isa<LastArrayAE>(this);
}

} // end comma namespace.

#endif
