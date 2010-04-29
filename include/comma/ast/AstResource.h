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
// like an identifier pool.
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

class AstResource {

public:
    AstResource(IdentifierPool &idPool);

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
    EnumerationDecl *createEnumSubtypeDecl(IdentifierInfo *name, Location loc,
                                           EnumerationType *subtype,
                                           Expr *low, Expr *high,
                                           DeclRegion *parent);

    /// Creates an unconstrained enumeration subtype declaration node.
    EnumerationDecl *createEnumSubtypeDecl(IdentifierInfo *name, Location loc,
                                           EnumerationType *subtype,
                                           DeclRegion *parent);

    /// Returns an EnumerationType node.
    EnumerationType *createEnumType(EnumerationDecl *decl);

    /// Returns an unconstrained subtype of the given EnumerationType.
    EnumerationType *createEnumSubtype(EnumerationType *base,
                                       EnumerationDecl *decl = 0);

    /// Returns a constrained enumeration subtype.
    EnumerationType *createEnumSubtype(EnumerationType *base,
                                       Expr *low, Expr *high,
                                       EnumerationDecl *decl = 0);
    //@}

    /// \name Integer declaration and type constructors.
    //@{
    /// Creates an integer declaration node.
    IntegerDecl *createIntegerDecl(IdentifierInfo *name, Location loc,
                                   Expr *lowRange, Expr *highRange,
                                   DeclRegion *parent);

    /// Creates a constrained integer subtype declaration node.
    IntegerDecl *createIntegerSubtypeDecl(IdentifierInfo *name, Location loc,
                                          IntegerType *subtype,
                                          Expr *lower, Expr *upper,
                                          DeclRegion *parent);

    /// Creates an unconstrained integer subtype declaration node.
    IntegerDecl *createIntegerSubtypeDecl(IdentifierInfo *name, Location loc,
                                          IntegerType *subtype,
                                          DeclRegion *parent);

    /// Returns an IntegerType node with the given static bounds.
    IntegerType *createIntegerType(IntegerDecl *decl,
                                   const llvm::APInt &low,
                                   const llvm::APInt &high);

    /// Returns an integer subtype node constrained over the given bounds.
    ///
    /// If \p decl is null an anonymous integer subtype is created.  Otherwise
    /// the subtype is associated with the given integer subtype declaration.
    IntegerType *createIntegerSubtype(IntegerType *base, Expr *low, Expr *high,
                                      IntegerDecl *decl = 0);

    /// Returns an anonymous integer subtype node constrained over the given
    /// bounds.
    ///
    /// The resulting subtype will have IntegerLiteral expressions generated to
    /// encapsulate the provided constants.
    ///
    /// If \o decl is null an anonymous integer subtype is created.  Otherwise
    /// the subtype is associated with the given integer subtype declaration.
    IntegerType *createIntegerSubtype(IntegerType *base,
                                      const llvm::APInt &low,
                                      const llvm::APInt &high,
                                      IntegerDecl *decl = 0);

    /// Returns an unconstrained integer subtype.
    IntegerType *createIntegerSubtype(IntegerType *base,
                                      IntegerDecl *decl = 0);
    //@}

    /// Creates a discrete subtype with the given bounds as constraints.
    ///
    /// The actual type returned depends on the actual type of the given base.
    DiscreteType *createDiscreteSubtype(DiscreteType *base,
                                        Expr *low, Expr *high,
                                        TypeDecl *decl = 0);

    /// \name Array declaration and type constructors.
    //@{
    /// Creates an Array declaration node.
    ArrayDecl *createArrayDecl(IdentifierInfo *name, Location loc,
                               unsigned rank, DSTDefinition **indices,
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

    /// Returns an anonymous constrained array subtype node.
    ArrayType *createArraySubtype(ArrayType *base, DiscreteType **indices);

    /// Returns an unconstrained array subtype node.
    ArrayType *createArraySubtype(IdentifierInfo *name, ArrayType *base);
    //@}

    /// \name Record declaration and type constructors.
    //@{
    /// Creates a Record declaration node.
    RecordDecl *createRecordDecl(IdentifierInfo *name, Location loc,
                                 DeclRegion *parent);

    /// Returns a record type corresponding to the given declaration.
    RecordType *createRecordType(RecordDecl *decl);

    /// Returns a subtype of the given record type.
    RecordType *createRecordSubtype(IdentifierInfo *name, RecordType *base);
    //@}

    /// \name Access declaration and type constructors.
    //@{
    /// Creates an access type declaration node.
    AccessDecl *createAccessDecl(IdentifierInfo *name, Location loc,
                                 Type *targetType, DeclRegion *parent);

    /// Creates an access type corresponding to the given declaration and target
    /// type node.
    AccessType *createAccessType(AccessDecl *decl, Type *targetType);

    /// Creates an access subtype using the given access type as a base.
    AccessType *createAccessSubtype(IdentifierInfo *name, AccessType *baseType);
    //@}

    /// \name Incomplete declaration and type constructors.
    //@{
    /// Creates an incomplete type declaration node.
    IncompleteTypeDecl *createIncompleteTypeDecl(IdentifierInfo *name,
                                                 Location loc,
                                                 DeclRegion *parent);

    /// Returns an incomplete type corresponding to the given declaration.
    IncompleteType *createIncompleteType(IncompleteTypeDecl *decl);

    /// Returns a subtype of the given incomplete type.
    IncompleteType *createIncompleteSubtype(IdentifierInfo *name,
                                            IncompleteType *base);
    //@}

    /// \brief Creates an exception declaration.
    ///
    /// \param name The name of this exception.
    ///
    /// \param loc The location of \p name in the source code.
    ///
    /// \param region The declarative region this exception was declared in.
    ExceptionDecl *createExceptionDecl(IdentifierInfo *name, Location loc,
                                       DeclRegion *region);

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
    /// defined type and exception declarations.  Eventually this interface will
    /// be superseded by a set of standard packages defining these nodes.
    //@{
    EnumerationDecl *getTheBooleanDecl() const { return theBooleanDecl; }
    EnumerationType *getTheBooleanType() const;

    EnumerationDecl *getTheCharacterDecl() const { return theCharacterDecl; }
    EnumerationType *getTheCharacterType() const;

    IntegerDecl *getTheRootIntegerDecl() const { return theRootIntegerDecl; }
    IntegerType *getTheRootIntegerType() const;

    IntegerDecl *getTheIntegerDecl() const { return theIntegerDecl; }
    IntegerType *getTheIntegerType() const;

    IntegerDecl *getTheNaturalDecl() const { return theNaturalDecl; }
    IntegerType *getTheNaturalType() const;

    IntegerDecl *getThePositiveDecl() const { return thePositiveDecl; }
    IntegerType *getThePositiveType() const;

    ArrayDecl *getTheStringDecl() const { return theStringDecl; }
    ArrayType *getTheStringType() const;

    ExceptionDecl *getTheProgramError() const { return theProgramError; }
    ExceptionDecl *getTheConstraintError() const { return theConstraintError; }
    ExceptionDecl *getTheAssertionError() const { return theAssertionError; }
    //@}

private:
    IdentifierPool &idPool;

    // Vectors of declaration and type nodes.
    std::vector<Decl*> decls;
    std::vector<Type*> types;

    // Tables of uniqued subroutine types.
    llvm::FoldingSet<FunctionType> functionTypes;
    llvm::FoldingSet<ProcedureType> procedureTypes;

    /// Subtype nodes corresponding to language defined types.

    /// Declaration nodes representing the language defined declarations and
    /// types.
    EnumerationDecl *theBooleanDecl;
    EnumerationDecl *theCharacterDecl;
    IntegerDecl *theRootIntegerDecl;
    IntegerDecl *theIntegerDecl;
    IntegerDecl *theNaturalDecl;
    IntegerDecl *thePositiveDecl;
    ArrayDecl *theStringDecl;
    ExceptionDecl *theProgramError;
    ExceptionDecl *theConstraintError;
    ExceptionDecl *theAssertionError;

    /// \name Language Defined Type Initialization.
    ///
    /// When an ASTResource is constructed, the complete set of AST nodes
    /// representing the language defined types and declarations are
    /// constructed.  This is implemented by the
    /// initializeLanguageDefinedNodes() method, which in turn calls a
    /// specialized method for each type/declaration.
    //@{
    void initializeLanguageDefinedNodes();
    void initializeBoolean();
    void initializeCharacter();
    void initializeRootInteger();
    void initializeInteger();
    void initializeNatural();
    void initializePositive();
    void initializeString();
    void initializeExceptions();
    //@}
};

} // End comma namespace.

#endif
