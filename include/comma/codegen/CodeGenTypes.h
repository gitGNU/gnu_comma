//===-- codegen/CodeGenTypes.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_CODEGENTYPES_HDR_GUARD
#define COMMA_CODEGEN_CODEGENTYPES_HDR_GUARD

#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/DerivedTypes.h"

namespace comma {

class CodeGen;

/// Lowers various Comma AST nodes to LLVM types.
///
/// As a rule, Comma type nodes need not in and of themselves provide enough
/// information to lower them directly to LLVM IR.  Thus, declaration nodes
/// are often needed so that the necessary information can be extracted from the
/// AST.
class CodeGenTypes {

public:
    CodeGenTypes(const CodeGen &CG) : CG(CG) { }

    const llvm::Type *lowerType(const Type *type);

    const llvm::Type *lowerDomainType(const DomainType *type);

    const llvm::Type *lowerCarrierType(const CarrierType *type);

    const llvm::IntegerType * lowerEnumType(const EnumerationType *type);

    const llvm::FunctionType *lowerSubroutine(const SubroutineDecl *decl);

    const llvm::Type *lowerSubType(const SubType *type);

    const llvm::IntegerType *lowerIntegerType(const IntegerType *type);

    const llvm::IntegerType *lowerIntegerSubType(const IntegerSubType *type) {
        return lowerIntegerType(type->getTypeOf());
    }

    const llvm::ArrayType *lowerArrayType(const ArrayType *type);

    const llvm::ArrayType *lowerArraySubType(const ArraySubType *type);

private:
    const CodeGen &CG;

    typedef std::pair<Type*, unsigned> RewriteVal;
    typedef llvm::DenseMap<Type*, RewriteVal> RewriteMap;
    RewriteMap rewrites;

    void addInstanceRewrites(DomainInstanceDecl *instance);
    void removeInstanceRewrites(DomainInstanceDecl *instance);
    const DomainType *rewriteAbstractDecl(AbstractDomainDecl *abstract);

    const llvm::IntegerType *getTypeForWidth(unsigned numBits);

    // Lowers the carrier type defined for the given domoid.
    const llvm::Type *lowerDomoidCarrier(const Domoid *domoid);

    /// Returns the number of elements for an array with a range bounded by the
    /// given values.
    uint64_t getArrayWidth(const llvm::APInt &low, const llvm::APInt &high);
};

}; // end comma namespace

#endif
