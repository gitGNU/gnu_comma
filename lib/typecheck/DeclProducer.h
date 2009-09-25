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
    DeclProducer(AstResource &resource);

    /// Returns the unique enumeration decl representing Bool.
    EnumerationDecl *getBoolDecl() const;

    /// Returns the unique enumeration type representing Bool.
    SubType *getBoolType() const;

    /// Returns the unique integer decl representing Integer.
    IntegerDecl *getIntegerDecl() const;

    /// Returns the unique SubType representing Integer.
    SubType *getIntegerType() const;

    /// Generates declarations appropriate for the given enumeration, populating
    /// \p enumDecl viewed as a DeclRegion with the results.
    void createImplicitDecls(EnumerationDecl *enumDecl);

    /// Generates declarations appropriate for the given integer declaration,
    /// populating \p intDecl viewed as a DeclRegion with the results.
    void createImplicitDecls(IntegerDecl *intDecl);

private:
    /// The resource we obtain AST's thru.
    AstResource &resource;

    /// The primitive Bool declaration node.
    EnumerationDecl *theBoolDecl;

    /// The primitive Integer declaration node.
    IntegerDecl *theIntegerDecl;

    /// The primitive Natural subtype node.
    IntegerSubType *theNaturalType;

    /// Constructor method for producing a raw Bool decl.  This function does
    /// not generate the associated implicit functions, however, the literals
    /// True and False are produced.
    void createTheBoolDecl();

    /// Constructor method for producing a raw Integer decl.  This function does
    /// not generate the associated implicit functions.
    void createTheIntegerDecl();

    /// Cunstructor method for producing the Natural subtype of Integer.
    void createTheNaturalDecl();

    /// Returns a function declaration for the given primitive operator.
    ///
    /// \param ID The primitive operator ID this declaration should support.
    ///
    /// \param type The argument types of this function.  If the operator is
    /// Boolean valued, the return type is Boolean.  If the operator is the
    /// exponentiation operator, the right hand type is Natural. Otherwise, this
    /// is an arithmetic operation and the return type matches the argument
    /// types.
    ///
    /// \param loc The location this operator is considered to be defined.
    ///
    /// \param region The decalarative region the function should be declared
    /// in.
    FunctionDecl *
    createPrimitiveDecl(PO::PrimitiveID ID, Location loc, Type *type,
                        DeclRegion *region);
};

} // end comma namespace.

#endif
