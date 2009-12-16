//===-- ast/DSTDefinition.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_DSTDEFINITION_HDR_GUARD
#define COMMA_AST_DSTDEFINITION_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/PointerIntPair.h"

namespace comma {

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Declares the DSTDefinition node.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \class
///
/// DSTDefinition nodes represent discrete subtype definitions.  These nodes are
/// used as the controlling specification in "for" statements and as index
/// constraints for array type declarations.
///
/// DSTDefinition nodes are essentially wrappers around either a DiscreteType or
/// RangeAttrib node.  The primary purpose is provide syntatic information about
/// how a type was described in source code and consolidate logic specific to
/// discrete type definitions.
class DSTDefinition : public Ast {

public:
    /// There are five syntatic methods by which a DSTDefinition can be
    /// described in source code.  The following enumeration encodes the method
    /// used.
    enum DSTTag {
        Attribute_DST,          ///< Range attribute.
        Range_DST,              ///< Simple range.
        Type_DST,               ///< Discrete type w/o constraint.
        Constrained_DST,        ///< Explicitly constrained discrete type.
        Unconstrained_DST       ///< Corresponds to "range <>".
    };

    /// Constructs a DSTDefinition over a generic AST node.  The supplied node
    /// must be either a RangeAttrib or a DiscreteType.  The former is only
    /// valid when \p tag is Attribute_DST.
    DSTDefinition(Location loc, Ast *base, DSTTag tag);

    /// Returns the associated DSTTag.
    DSTTag getTag() const { return static_cast<DSTTag>(bits); }

    /// Returns the location of this node.
    Location getLocation() const { return loc; }

    //@{
    /// Returns the discrete type defined by this DSTDefinition.
    const DiscreteType *getType() const {
        return const_cast<DSTDefinition*>(this)->getType();
    }
    DiscreteType *getType();
    //@}

    /// Returns true if this DSTDefinition was specified using a range.
    bool definedUsingRange() const { return getTag() == Range_DST; }

    //@{
    /// When definedUsingRange is true, returns the range associated with this
    /// DSTDefinition.
    const Range *getRange() const;
    Range *getRange();
    //@}

    /// Returns true if this DSTDefinition was specified using a range
    /// attribute.
    bool definedUsingAttrib() const { return getTag() == Attribute_DST; }

    //@{
    /// When definedUsingAttib is true, returns the RangeAttrib associated with
    /// this DSTDefinition.
    //@{
    const RangeAttrib *getAttrib() const;
    RangeAttrib *getAttrib();
    //@}

    /// Returns true if this DSTDefinition was specified using a simple subtype
    /// mark.
    ///
    /// \note When a discrete subtype is supplied to the constructor, this is
    /// the resulting state.  A DSTDefinition is `promoted' to a constrained
    /// definition with a call to addConstraint().
    bool definedUsingSubtype() const { return getTag() == Type_DST; }

    /// Returns true if this DSTDefinition was specified using an explicity
    /// constrained subtype indication.
    bool definedUsingConstraint() const { return getTag() == Constrained_DST; }

    /// Returns true if this DSTDefinition was specified as an unconstrained
    /// index type.
    bool definedUsingDiamond() const { return getTag() == Unconstrained_DST; }

    // Support isa/dyn_cast.
    static bool classof(const DSTDefinition *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_DSTDefinition;
    }

private:
    Location loc;
    Ast *definition;
};

} // end comma namespace.

#endif
