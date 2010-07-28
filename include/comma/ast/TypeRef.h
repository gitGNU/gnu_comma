//===-- ast/TypeRef.h ----------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file defines the TypeRef class.
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_TYPEREF_HDR_GUARD
#define COMMA_AST_TYPEREF_HDR_GUARD

namespace comma {

#include "comma/ast/Decl.h"

/// The TypeRef class is a miscellaneous AST node, meaning that it does not
/// belong to any of the major AST hierarchy branches.  It is a thin wrapper
/// around a specific named type, associating a location with the reference.
///
/// These nodes are transient and do not find a home in the final AST.  They are
/// created to hold information about references to type declarations while the
/// type checker is being driven by the parser.
class TypeRef : public Ast {

public:
    /// Creates a reference to the given type declaration.
    TypeRef(Location loc, TypeDecl *tyDecl)
        : Ast(AST_TypeRef), loc(loc), target(tyDecl) { }

    /// Returns the location of this reference.
    Location getLocation() const { return loc; }

    /// Returns the IdentifierInfo of the underlying decl.
    IdentifierInfo *getIdInfo() const { return target->getIdInfo(); }

    //@{
    /// If this references a type declaration, return it, else null.
    const TypeDecl *getDecl() const { return target; }
    TypeDecl *getDecl() { return target; }
    //@}

    //@{
    /// Returns the referenced type.
    const Type *getType() const { return target->getType(); }
    Type *getType() { return target->getType(); }
    //@}

    // Support isa and dyn_cast.
    static bool classof(const TypeRef *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_TypeRef;
    }

private:
    Location loc;
    TypeDecl *target;
};

} // end comma namespace.

#endif
