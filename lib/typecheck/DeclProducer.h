//===-- typecheck/DeclProducer.h ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// The DeclProducer class encapsulates actions needed by the typechecker to
// produce declarations.
//
// One such category of actions is the generation of implicit declarations
// representing the primitive types predefined by Comma.  Unique declaration
// nodes representing types such as Bool and Integer are generated and
// accessable thru this class.
//
// Another category are actions which automatically generate declarations
// implicitly created in support of certain type definitions.  For example,
// enumeration types define equality and comparison functions, and integer types
// define a host of arithmetic functions.  Methods are provided for constructing
// the needed AST's.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_DECLPRODUCER_HDR_GUARD
#define COMMA_TYPECHECK_DECLPRODUCER_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/basic/PrimitiveOps.h"

namespace comma {

class AstResource;

class DeclProducer {

public:
    DeclProducer(AstResource *resource);

    /// Returns the unique enumeration decl representing Bool.
    EnumerationDecl *getBoolDecl() const;

    /// Returns the unique enumeration type representing Bool.
    EnumerationType *getBoolType() const;

    /// Returns the unique integer decl representing Integer.
    IntegerDecl *getIntegerDecl() const;

    /// Returns the unique TypedefType representing Integer.
    TypedefType *getIntegerType() const;

    /// Generates declarations appropriate for the given enumeration, populating
    /// \p enumDecl viewed as a DeclRegion with the results.
    void createImplicitDecls(EnumerationDecl *enumDecl);

    /// Generates declarations appropriate for the given integer declaration,
    /// populating \p region viewed as a DeclRegion with the results.
    void createImplicitDecls(IntegerDecl *intDecl);

private:
    /// The resource we obtain AST's thru.
    AstResource *resource;

    /// The primitive Bool declaration node.
    EnumerationDecl *theBoolDecl;

    /// The primitive Integer declaration node.
    IntegerDecl *theIntegerDecl;

    /// Constructor method for producing a raw Bool decl.  This function does
    /// not generate the associated implicit functions, however, the literals
    /// True and False are produced.
    void createTheBoolDecl();

    /// Constructor method for producing a raw Integer decl.  This function does
    /// not generate the associated implicit functions.
    void createTheIntegerDecl();

    /// An enumeration itemizing the various types of predicate functions we can
    /// produce.  Used as an argument to DeclProducer::createBinaryPredicate.
    enum PredicateKind {
        EQ_pred,
        LT_pred,
        GT_pred,
        LTEQ_pred,
        GTEQ_pred,
    };

    /// Returns an IdentifierInfo nameing the given predicate.
    IdentifierInfo *getPredicateName(PredicateKind kind);

    /// Returns the primitive operation marker for the given predicate.
    PO::PrimitiveID getPredicatePrimitive(PredicateKind kind);

    /// Generates a binary predicate function.
    ///
    /// The function is named after the Comma operator for the given type of
    /// predicate.  For example, \c EQ_pred results in a function names "=",
    /// while LTEQ_pred results in a function named "<=".  All predicate
    /// functions have argument selectors named "X" and "Y".  And, obviously,
    /// the return type is Bool.  The parent declarative region of the resulting
    /// function decl is set to \p region.
    FunctionDecl *createPredicate(PredicateKind kind, Type *paramType,
                                  DeclRegion *parent);
};

} // end comma namespace.

#endif
