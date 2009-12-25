//===-- codegen/CodeGenTypes.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENTYPES_HDR_GUARD
#define COMMA_CODEGEN_CODEGENTYPES_HDR_GUARD

#include "comma/ast/AstBase.h"

#include "llvm/ADT/ScopedHashTable.h"
#include "llvm/DerivedTypes.h"

namespace comma {

class CGContext;
class CodeGen;

/// Lowers various Comma AST nodes to LLVM types.
///
/// As a rule, Comma type nodes need not in and of themselves provide enough
/// information to lower them directly to LLVM IR.  Thus, declaration nodes are
/// often needed so that the necessary information can be extracted from the
/// AST.
class CodeGenTypes {

public:
    CodeGenTypes(CodeGen &CG, DomainInstanceDecl *context = 0)
        : CG(CG), topScope(rewrites), context(context) {
        if (context)
            addInstanceRewrites(context);
    };

    const llvm::Type *lowerType(const Type *type);

    const llvm::Type *lowerDomainType(const DomainType *type);

    const llvm::IntegerType *lowerEnumType(const EnumerationType *type);

    const llvm::FunctionType *lowerSubroutine(const SubroutineDecl *decl);

    const llvm::IntegerType *lowerDiscreteType(const DiscreteType *type);

    const llvm::ArrayType *lowerArrayType(const ArrayType *type);

    /// Returns the structure type used to hold the bounds of an unconstrained
    /// array.
    const llvm::StructType *lowerArrayBounds(const ArrayType *arrTy);

    /// Returns the structure type used to hold the bounds of the given scalar
    /// type.
    const llvm::StructType *lowerScalarBounds(const DiscreteType *type);

    /// Returns the structure type used to hold the bounds of the given range.
    const llvm::StructType *lowerRange(const Range *range);

private:
    CodeGen &CG;

    typedef llvm::ScopedHashTable<const Type*, const Type*> RewriteMap;
    typedef llvm::ScopedHashTableScope<const Type*, const Type*> RewriteScope;
    RewriteMap rewrites;
    RewriteScope topScope;

    DomainInstanceDecl *context;

    const DomainType *rewriteAbstractDecl(const AbstractDomainDecl *abstract);

    const llvm::IntegerType *getTypeForWidth(unsigned numBits);

    void addInstanceRewrites(const DomainInstanceDecl *instance);
};

}; // end comma namespace

#endif
