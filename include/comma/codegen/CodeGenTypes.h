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

    const llvm::IntegerType *lowerTypedefType(const TypedefType *type);

    const llvm::IntegerType *lowerIntegerType(const IntegerType *type);

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
};

}; // end comma namespace

#endif
