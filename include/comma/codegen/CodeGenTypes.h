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

#include "llvm/DerivedTypes.h"
#include "llvm/Target/TargetData.h"

namespace comma {

class CodeGen;

// This class is responsible for lowering Comma AST types to LLVM IR types.
class CodeGenTypes {

public:
    CodeGenTypes(const CodeGen &CG) : CG(CG) { }

    const llvm::Type *lowerType(Type *type);

    const llvm::Type *lowerType(DomainType *type);
    const llvm::Type *lowerType(CarrierType *type);
    const llvm::IntegerType *lowerType(EnumerationType *type);
    const llvm::FunctionType *lowerType(const SubroutineType *type);
    const llvm::IntegerType *lowerType(const TypedefType *type);
    const llvm::IntegerType *lowerType(const IntegerType *type);

private:
    const CodeGen &CG;

    const llvm::IntegerType *getTypeForWidth(unsigned numBits);
};

}; // end comma namespace

#endif
