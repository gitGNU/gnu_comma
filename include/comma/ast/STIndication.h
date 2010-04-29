//===-- ast/STIndication.h ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_STINDICATION_HDR_GUARD
#define COMMA_AST_STINDICATION_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

/// \class
/// \brief Representation of subtype indications.
class STIndication : public Ast
{
public:
    /// Creates a subtype indication.
    STIndication(Location loc, PrimaryType *subtype, bool isNotNull = false)
        : Ast(AST_STIndication), loc(loc), subtypeMark(subtype) {
        bits = isNotNull;
    }

    //@{
    /// Returns the type associated with this subtype indication.
    const PrimaryType *getType() const { return subtypeMark; }
    PrimaryType *getType() { return subtypeMark; }
    //@}

    /// Returns the location of this subtype indication.
    Location getLocation() const { return loc; }

    /// Returns true if this subtype indication includes a constraint.
    bool isConstrained() const { return subtypeMark->isConstrained(); }

    /// Returns true if this subtype indication includes a null exclusion.
    bool isNotNull() const { return bits != 0; }

    /// Marks this subtype indication with a null exclusion.
    void setNotNull() { bits = 1; }

    // Support isa/dyn_cast.
    static bool classof(const STIndication *node) {
        return true;
    }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_STIndication;
    }

private:
    // Location of this STIndication.
    Location loc;

    // The subtype mark associated with this STIndication.
    PrimaryType *subtypeMark;
};

} // end comma namespace.

#endif


