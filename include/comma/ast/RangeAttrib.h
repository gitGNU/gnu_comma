//===-- ast/RangeAttrib.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_RANGEATTRIB_HDR_GUARD
#define COMMA_AST_RANGEATTRIB_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Expr.h"

namespace comma {

//===----------------------------------------------------------------------===//
// RangeAttrib
//
/// Base class for range attribute nodes.
class RangeAttrib : public Ast {

public:
    //@{
    /// Returns the prefix to which this attribute applies.
    ///
    /// The returned node is either a TypeRef, SubroutineRef, or an Expr.
    const Ast *getPrefix() const { return prefix; }
    Ast *getPrefix() { return prefix; }
    //@}

    /// Returns the location of this attribute.
    Location getLocation() const { return loc; }

    //@{
    /// Returns the type of this range attribute.
    virtual const DiscreteType *getType() const = 0;
    virtual DiscreteType *getType() = 0;
    //@}

    // Support isa/dyn_cast.
    static bool classof(const RangeAttrib *node) { return true; }
    static bool classof(const Ast *node) {
        return denotesRangeAttrib(node->getKind());
    }

protected:
    RangeAttrib(AstKind kind, Ast *prefix, Location loc)
        : Ast(kind), prefix(prefix), loc(loc) { }

    static bool denotesRangeAttrib(AstKind kind) {
        return (kind == AST_ScalarRangeAttrib || kind == AST_ArrayRangeAttrib);
    }

private:
    Ast *prefix;
    Location loc;
};

//===----------------------------------------------------------------------===//
// ArrayRangeAttrib
//
/// This node representes range attributes when applied to a prefix expression
/// of array type.
class ArrayRangeAttrib : public RangeAttrib
{
public:
    /// Constructs an array range attribute with an explicit dimension.
    ///
    /// The dimension must be staticly evaluable to a discrete value.
    ArrayRangeAttrib(Expr *prefix, Expr *dimension, Location loc)
        : RangeAttrib(AST_ArrayRangeAttrib, prefix, loc),
          dimExpr(dimension) {
        assert(llvm::isa<ArrayType>(prefix->getType()) &&
               "Invalid prefix for array range attribute!");
        assert(dimension->isStaticDiscreteExpr() &&
               "Non-static dimension given to range attribute!");
        llvm::APInt value;
        dimension->staticDiscreteValue(value);
        dimValue = value.getZExtValue();
    }

    /// Constructs an array range attribute with an implicit dimension.
    ArrayRangeAttrib(Expr *prefix, Location loc)
        : RangeAttrib(AST_ArrayRangeAttrib, prefix, loc),
          dimExpr(0), dimValue(0) {
        assert(llvm::isa<ArrayType>(prefix->getType()) &&
               "Invalid prefix for array range attribute!");
    }

    //@{
    /// Returns the prefix of this range attribute.
    const Expr *getPrefix() const {
        return llvm::cast<Expr>(RangeAttrib::getPrefix());
    }
    Expr *getPrefix() {
        return llvm::cast<Expr>(RangeAttrib::getPrefix());
    }
    //@}

    /// Returns true if the dimension associated with this attribute is
    /// implicit.
    bool hasImplicitDimension() const { return dimExpr == 0; }

    //@{
    /// Returns the dimension expression associated with this attribute, or null
    /// if the dimension is implicit for this node.
    ///
    /// Note the the expression returned is always a static
    Expr *getDimensionExpr() { return dimExpr; }
    const Expr *getDimensionExpr() const { return dimExpr; }
    //@}

    /// Returns the zero based dimension associated with this attribute.
    unsigned getDimension() const { return dimValue; }

    //@{
    /// Returns the type of this range attribute.
    const DiscreteType *getType() const {
        const ArrayType *arrTy = llvm::cast<ArrayType>(getPrefix()->getType());
        return arrTy->getIndexType(dimValue);
    };

    DiscreteType *getType() {
        ArrayType *arrTy = llvm::cast<ArrayType>(getPrefix()->getType());
        return arrTy->getIndexType(dimValue);
    }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ArrayRangeAttrib *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ArrayRangeAttrib;
    }

private:
    /// The expression node defining the associated dimension, or null if the
    /// dimension is implicit.
    Expr *dimExpr;

    /// The static value of the dimension expression, or 0 if the dimension is
    /// implicit.
    unsigned dimValue;
};

//===----------------------------------------------------------------------===//
// ScalarRangeAttrib
//
/// This node represents range attributes when applied to a scalar type prefix.
class ScalarRangeAttrib : public RangeAttrib
{
public:
    ScalarRangeAttrib(DiscreteType *prefix, Location loc)
        : RangeAttrib(AST_ScalarRangeAttrib, prefix, loc) { }

    //@{
    /// Returns the prefix of this range attribute.
    const DiscreteType *getPrefix() const {
        return llvm::cast<DiscreteType>(RangeAttrib::getPrefix());
    }
    DiscreteType *getPrefix() {
        return llvm::cast<DiscreteType>(RangeAttrib::getPrefix());
    }
    //@}

    //@{
    /// Returns the type of this range attribute.
    const DiscreteType *getType() const { return getPrefix(); }
    DiscreteType *getType() { return getPrefix(); }
    //@}

    // Support isa/dyn_cast.
    static bool classof(const ScalarRangeAttrib *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_ScalarRangeAttrib;
    }
};

} // end comma namespace.

#endif
