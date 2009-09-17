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
/// A TypeRef is said to be `incomplete' if the ref names a parameterized type
/// such as a Variety or Functor.  A `complete' ref is one which names a
/// non-parameterized type, or a fully applied instance of a parameterized type.
///
/// These nodes are transient and do not find a home in the final AST.  They are
/// created to hold information about references to type declarations while the
/// type checker is being driven by the parser.
class TypeRef : public Ast {

public:
    /// Creates an incomplete reference to the given functor.
    TypeRef(Location loc, FunctorDecl *functor)
        : Ast(AST_TypeRef), loc(loc), target(functor) { }

    /// Creates a incomplete reference to the given variety.
    TypeRef(Location loc, VarietyDecl *variety)
        : Ast(AST_TypeRef), loc(loc), target(variety) { }

    /// Creates a complete reference to the given signature instance.
    TypeRef(Location loc, SigInstanceDecl *instance)
        : Ast(AST_TypeRef), loc(loc), target(instance) { }

    /// Creates a complete reference to the given type declaration.
    TypeRef(Location loc, TypeDecl *tyDecl)
        : Ast(AST_TypeRef), loc(loc), target(tyDecl) { }

    /// Returns the location of this reference.
    Location getLocation() const { return loc; }

    /// Returns the IdentifierInfo of the underlying decl.
    IdentifierInfo *getIdInfo() const { return target->getIdInfo(); }

    /// Returns true if this is a incomplete reference.
    bool isIncomplete() const {
        return llvm::isa<FunctorDecl>(target) || llvm::isa<VarietyDecl>(target);
    }

    /// Returns true if this is a complete reference.
    bool isComplete() const { return !isIncomplete(); }

    /// Returns true if this is an incomplete variety reference.
    bool referencesVariety() const {
        return llvm::isa<VarietyDecl>(target);
    }

    /// Returns true if this is an incomplete functor reference.
    bool referencesFunctor() const {
        return llvm::isa<FunctorDecl>(target);
    }

    /// Returns true if this is a complete reference to a signature instance.
    bool referencesSigInstance() const {
        return llvm::isa<SigInstanceDecl>(target);
    }

    /// Returns true if this is a complete reference to a TypeDecl.
    bool referencesTypeDecl() const {
        return llvm::isa<TypeDecl>(target);
    }

    /// Returns the referenced declaration.
    Decl *getDecl() const { return target; }

    /// If this references a variety or functor, return it as a general
    /// ModelDecl, else null.
    ModelDecl *getModelDecl() const {
        return llvm::dyn_cast<ModelDecl>(target);
    }

    /// If this references a variety, return it, else null.
    VarietyDecl *getVarietyDecl() const {
        return llvm::dyn_cast<VarietyDecl>(target);
    }

    /// If this references a functor, return it, else null.
    FunctorDecl *getFunctorDecl() const {
        return llvm::dyn_cast<FunctorDecl>(target);
    }

    /// If this references a signature instance, return it, else null.
    SigInstanceDecl *getSigInstanceDecl() const {
        return llvm::dyn_cast<SigInstanceDecl>(target);
    }

    /// If this references a type declaration, return it, else null.
    TypeDecl *getTypeDecl() const {
        return llvm::dyn_cast<TypeDecl>(target);
    }

    // Support isa and dyn_cast.
    static bool classof(const TypeRef *node) { return true; }
    static bool classof(const Ast *node) {
        return node->getKind() == AST_TypeRef;
    }

private:
    Location loc;
    Decl *target;
};

} // end comma namespace.

#endif
