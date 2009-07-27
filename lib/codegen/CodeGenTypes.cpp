//===-- codegen/CodeGenTypes.cpp ------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/codegen/CodeGen.h"
#include "comma/codegen/CodeGenTypes.h"
#include "comma/codegen/CommaRT.h"

#include "llvm/DerivedTypes.h"
#include "llvm/Support/MathExtras.h"

using namespace comma;

using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;

const llvm::Type *CodeGenTypes::lowerType(Type *type)
{
    switch (type->getKind()) {

    default:
        assert(false && "Cannot lower the given Type!");
        return 0;

    case Ast::AST_EnumerationType:
        return lowerType(cast<EnumerationType>(type));

    case Ast::AST_FunctionType:
    case Ast::AST_ProcedureType:
        return lowerType(cast<SubroutineType>(type));
    }
}

const llvm::IntegerType *CodeGenTypes::lowerType(EnumerationType *type)
{
    // Enumeration types are lowered to an integer type with sufficient capacity
    // to hold each element of the enumeration.
    EnumerationDecl *decl = type->getEnumerationDecl();
    unsigned numBits = 32 - llvm::CountLeadingZeros_32(decl->getNumLiterals() - 1);
    return llvm::IntegerType::get(numBits);
}

const llvm::FunctionType *CodeGenTypes::lowerType(SubroutineType *type)
{
    std::vector<const llvm::Type*> args;
    const llvm::FunctionType *result;

    // Emit the implicit first "%" argument.
    args.push_back(CG.getRuntime().getType(CommaRT::CRT_DomainInstance));

    for (unsigned i = 0; i < type->getArity(); ++i)
        args.push_back(lowerType(type->getArgType(i)));

    if (FunctionType *ftype = dyn_cast<FunctionType>(type))
        result = llvm::FunctionType::get(lowerType(ftype->getReturnType()), args, false);
    else
        result = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);

    return result;
}



