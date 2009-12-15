//===-- ast/AstResource.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// The AstResource class provides access to "universal" resources necessary for
// builing and managing Ast trees.  It provides mechanisms for managing the
// allocation of nodes, a single point of contact for fundamental facilities
// like text providers and an identifier pool.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_ASTRESOURCE_HDR_GUARD
#define COMMA_AST_ASTRESOURCE_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/basic/IdentifierPool.h"
#include "comma/basic/PrimitiveOps.h"

#include "llvm/ADT/FoldingSet.h"

#include <vector>

namespace llvm {

class APInt;

} // end llvm namespace.

namespace comma {

class IndexConstraint;
class RangeConstraint;
class TextProvider;

class AstResource {

public:
    // For now, this class is simply a bag of important classes: A TextProvider
    // associated with the file being processed, and an IdentifierPool for the
    // global managment of identifiers.
    AstResource(TextProvider &txtProvider, IdentifierPool &idPool);

    // FIXME: Eventually we will replace this single TextProvider resource with
    // a manager class which provides services to handle multiple input files.
    TextProvider &getTextProvider() { return txtProvider; }

    IdentifierPool &getIdentifierPool() { return idPool; }

    // Convenience function to extract an IdentifierInfo object from the
    // associated IdentifierPool.
    IdentifierInfo *getIdentifierInfo(const char *name) const {
        return &idPool.getIdentifierInfo(name);
    }
    IdentifierInfo *getIdentifierInfo(const char *name, unsigned len) const {
        return &idPool.getIdentifierInfo(name, len);
    }
    IdentifierInfo *getIdentifierInfo(const std::string &name) const {
        return &idPool.getIdentifierInfo(name);
    }

    /// Returns a uniqued FunctionType.
    FunctionType *getFunctionType(Type **argTypes, unsigned numArgs,
                                  Type *returnType);

    /// Returns a uniqued ProcedureType.
    ProcedureType *getProcedureType(Type **argTypes, unsigned numArgs);

    /// \name Domain type constructors.
    //@{

    /// Creates a DomainType node.
    ///
    /// Constructs a root domain type and its first subtype.  Returns the first
    /// subtype.
    DomainType *createDomainType(DomainTypeDecl *decl);

    /// Creates a subtype of the given domain type node.
    DomainType *createDomainSubtype(DomainType *rootTy, IdentifierInfo *name);
    //@}

    /// \name Enumeration declaration and type constructors.
    //@{
    /// Creates an enumeration declaration node.
    EnumerationDecl *createEnumDecl(IdentifierInfo *name, Location loc,
                                    std::pair<IdentifierInfo*, Location> *elems,
                                    unsigned numElems, DeclRegion *parent);

    /// Creates a constrained enumeration subtype declaration node.
    EnumSubtypeDecl *createEnumSubtypeDecl(IdentifierInfo *name, Location loc,
                                           EnumerationType *subtype,
                                           Expr *low, Expr *high,
                                           DeclRegion *parent);

    /// Creates an unconstrained enumeration subtype declaration node.
    EnumSubtypeDecl *createEnumSubtypeDecl(IdentifierInfo *name, Location loc,
                                           EnumerationType *subtype,
                                           DeclRegion *parent);

    /// Returns an EnumerationType node.
    EnumerationType *createEnumType(EnumerationDecl *decl);

    /// Returns an unconstrained subtype of the given EnumerationType.
    EnumerationType *createEnumSubtype(IdentifierInfo *name,
                                       EnumerationType *base);

    /// Returns a constrained enumeration subtype.
    EnumerationType *createEnumSubtype(IdentifierInfo *name,
                                       EnumerationType *base,
                                       Expr *low, Expr *high);
    //@}

    /// \name Integer declaration and type constructors.
    //@{
    /// Creates an integer declaration node.
    IntegerDecl *createIntegerDecl(IdentifierInfo *name, Location loc,
                                   Expr *lowRange, Expr *highRange,
                                   DeclRegion *parent);

    /// Creates a constrained integer subtype declaration node.
    IntegerSubtypeDecl *
    createIntegerSubtypeDecl(IdentifierInfo *name, Location loc,
                             IntegerType *subtype,
                             Expr *lower, Expr *upper,
                             DeclRegion *parent);

    /// Creates an unconstrained integer subtype declaration node.
    IntegerSubtypeDecl *
    createIntegerSubtypeDecl(IdentifierInfo *name, Location loc,
                             IntegerType *subtype,
                             DeclRegion *parent);

    /// Returns an IntegerType node with the given static bounds.
    IntegerType *createIntegerType(IntegerDecl *decl,
                                   const llvm::APInt &low,
                                   const llvm::APInt &high);

    /// Returns an integer subtype node with the given bounds.
    IntegerType *createIntegerSubtype(IdentifierInfo *name,
                                      IntegerType *base,
                                      Expr *low, Expr *high);

    /// Returns an integer subtype node constrained to the given bounds.
    ///
    /// The resulting subtype will have IntegerLiteral expressions generated to
    /// encapsulate the provided constants.
    IntegerType *createIntegerSubtype(IdentifierInfo *name,
                                      IntegerType *base,
                                      const llvm::APInt &low,
                                      const llvm::APInt &high);

    /// Returns an unconstrained integer subtype.
    IntegerType *createIntegerSubtype(IdentifierInfo *name,
                                      IntegerType *base);
    //@}

    /// Creates a discrete subtype with the given bounds as constraints.
    ///
    /// The actual type returned depends on the actual type of the given base.
    DiscreteType *createDiscreteSubtype(IdentifierInfo *name,
                                        DiscreteType *base,
                                        Expr *low, Expr *high);

    /// \name Array declaration and type constructors.
    //@{
    /// Creates an Array declaration node.
    ArrayDecl *createArrayDecl(IdentifierInfo *name, Location loc,
                               unsigned rank, DiscreteType **indices,
                               Type *component, bool isConstrained,
                               DeclRegion *parent);

    /// Returns an ArrayType node with the given index and component
    /// types.
    ArrayType *createArrayType(ArrayDecl *decl,
                               unsigned rank, DiscreteType **indices,
                               Type *component, bool isConstrained);

    /// Returns a constrained array subtype node.
    ArrayType *createArraySubtype(IdentifierInfo *name, ArrayType *base,
                                  DiscreteType **indices);

    /// Returns an unconstrained array subtype node.
    ArrayType *createArraySubtype(IdentifierInfo *name, ArrayType *base);
    //@}

    /// Creates a function declaration corresponding to the given primitive
    /// operation.
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
    FunctionDecl *createPrimitiveDecl(PO::PrimitiveID ID,
                                      Location loc, Type *type,
                                      DeclRegion *region);

    /// \name Language Defined AST Nodes.
    ///
    ///
    /// Provides access to basic nodes which represent the various language
    /// defined type declarations, or in the absence of a declaration, the
    /// primitive type.
    //@{
    EnumerationDecl *getTheBooleanDecl() const { return theBooleanDecl; }
    EnumerationType *getTheBooleanType() const;

    EnumerationDecl *getTheCharacterDecl() const { return theCharacterDecl; }
    EnumerationType *getTheCharacterType() const;

    IntegerDecl *getTheRootIntegerDecl() const { return theRootIntegerDecl; }
    IntegerType *getTheRootIntegerType() const;

    IntegerDecl *getTheIntegerDecl() const { return theIntegerDecl; }
    IntegerType *getTheIntegerType() const;

    IntegerSubtypeDecl *getTheNaturalDecl() const { return theNaturalDecl; }
    IntegerType *getTheNaturalType() const;

    IntegerSubtypeDecl *getThePositiveDecl() const { return thePositiveDecl; }
    IntegerType *getThePositiveType() const;

    ArrayDecl *getTheStringDecl() const { return theStringDecl; }
    ArrayType *getTheStringType() const;
    //@}

private:
    TextProvider &txtProvider;
    IdentifierPool &idPool;

    // Vectors of declaration and type nodes.
    std::vector<Decl*> decls;
    std::vector<Type*> types;

    // Tables of uniqued subroutine types.
    llvm::FoldingSet<FunctionType> functionTypes;
    llvm::FoldingSet<ProcedureType> procedureTypes;

    /// Subtype nodes corresponding to language defined types.

    /// Declaration nodes representing the language defined types.
    EnumerationDecl *theBooleanDecl;
    EnumerationDecl *theCharacterDecl;
    IntegerDecl *theRootIntegerDecl;
    IntegerDecl *theIntegerDecl;
    IntegerSubtypeDecl *theNaturalDecl;
    IntegerSubtypeDecl *thePositiveDecl;
    ArrayDecl *theStringDecl;

    /// \name Language Defined Type Initialization.
    ///
    /// When an ASTResource is constructed, the complete set of AST nodes
    /// representing the language defined types are constructed.  This is
    /// implemented by the initializeLanguageDefinedTypes() method, which in
    /// turn calls a specialized method for each type.
    //@{
    void initializeLanguageDefinedTypes();
    void initializeBoolean();
    void initializeCharacter();
    void initializeRootInteger();
    void initializeInteger();
    void initializeNatural();
    void initializePositive();
    void initializeString();
    //@}
};

} // End comma namespace.

#endif
