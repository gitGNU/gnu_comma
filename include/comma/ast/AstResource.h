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

#include "llvm/ADT/FoldingSet.h"

namespace llvm {

class APInt;

} // end llvm namespace.

namespace comma {

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

    /// Returns a uniqued FunctionType.
    FunctionType *getFunctionType(Type **argTypes, unsigned numArgs,
                                  Type *returnType);

    /// Returns a uniqued ProcedureType.
    ProcedureType *getProcedureType(Type **argTypes, unsigned numArgs);

    /// Returns a uniqued IntegerType node of the specified range.
    IntegerType *getIntegerType(const llvm::APInt &low,
                                const llvm::APInt &high);

    /// Returns a uniqued ArrayType node with the given index and component
    /// types.
    ArrayType *getArrayType(unsigned rank, Type **indices, Type *component);

private:
    TextProvider &txtProvider;
    IdentifierPool &idPool;

    llvm::FoldingSet<FunctionType> functionTypes;
    llvm::FoldingSet<ProcedureType> procedureTypes;
    llvm::FoldingSet<IntegerType> integerTypes;
    llvm::FoldingSet<ArrayType> arrayTypes;
};

} // End comma namespace.

#endif
