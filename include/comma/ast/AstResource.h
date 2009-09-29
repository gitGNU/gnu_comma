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
    IdentifierInfo *getIdentifierInfo(const std::string &name) const {
        return &idPool.getIdentifierInfo(name);
    }

    /// Returns a uniqued FunctionType.
    FunctionType *getFunctionType(Type **argTypes, unsigned numArgs,
                                  Type *returnType);

    /// Returns a uniqued ProcedureType.
    ProcedureType *getProcedureType(Type **argTypes, unsigned numArgs);

    /// \name Enumeration declaration and type constructors.
    //@{
    /// Creates an enumeration declaration node.
    EnumerationDecl *createEnumDecl(IdentifierInfo *name, Location loc,
                                    std::pair<IdentifierInfo*, Location> *elems,
                                    unsigned numElems, DeclRegion *parent);

    /// Returns an EnumerationType node.
    EnumerationType *createEnumType(unsigned numElements);

    /// Returns a subtype of the given EnumerationType.
    EnumSubType *createEnumSubType(IdentifierInfo *name, EnumerationType *base);
    //@}

    /// \name Integer declaration and type constructors.
    //@{
    /// Creates an integer declaration node.
    IntegerDecl *createIntegerDecl(IdentifierInfo *name, Location loc,
                                   Expr *lowRange, Expr *highRange,
                                   const llvm::APInt &lowVal,
                                   const llvm::APInt &highVal,
                                   DeclRegion *parent);

    /// Returns an IntegerType node with the given static bounds.
    IntegerType *createIntegerType(IntegerDecl *decl,
                                   const llvm::APInt &low,
                                   const llvm::APInt &high);

    /// Returns an IntegerSubType node with the given static bounds.
    IntegerSubType *createIntegerSubType(IdentifierInfo *name,
                                         IntegerType *base,
                                         const llvm::APInt &low,
                                         const llvm::APInt &high);

    /// Returns an unconstrained IntegerSubType.
    IntegerSubType *createIntegerSubType(IdentifierInfo *name,
                                         IntegerType *base);
    //@}

    /// \name Array declaration and type constructors.
    //@{
    /// Creates an Array declaration node.
    ArrayDecl *createArrayDecl(IdentifierInfo *name, Location loc,
                               unsigned rank, SubType **indices,
                               Type *component, bool isConstrained,
                               DeclRegion *parent);

    /// Returns an ArrayType node with the given index and component
    /// types.
    ArrayType *createArrayType(unsigned rank,
                               SubType **indices, Type *component,
                               bool isConstrained);

    /// Returns an ArraySubType node.
    ArraySubType *createArraySubType(IdentifierInfo *name, ArrayType *base,
                                     IndexConstraint *constraint);
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
    EnumSubType *getTheBooleanType() const;

    IntegerDecl *getTheRootIntegerDecl() const { return theRootIntegerDecl; }
    IntegerSubType *getTheRootIntegerType() const;

    IntegerDecl *getTheIntegerDecl() const { return theIntegerDecl; }
    IntegerSubType *getTheIntegerType() const;

    IntegerSubType *getTheNaturalType() const { return theNaturalType; }
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

    /// SubType nodes corresponding to language defined types.
    IntegerSubType *theNaturalType;

    /// Declaration nodes representing the language defined types.
    EnumerationDecl *theBooleanDecl;
    IntegerDecl *theRootIntegerDecl;
    IntegerDecl *theIntegerDecl;

    /// \name Language Defined Type Initialization.
    ///
    /// When an ASTResource is constructed, the complete set of AST nodes
    /// representing the language defined types are constructed.  This is
    /// implemented by the initializeLanguageDefinedTypes() method, which in
    /// turn calls a specialized method for each type.
    //@{
    void initializeLanguageDefinedTypes();
    void initializeBoolean();
    void initializeRootInteger();
    void initializeInteger();
    void initializeNatural();
    //@}
};

} // End comma namespace.

#endif
