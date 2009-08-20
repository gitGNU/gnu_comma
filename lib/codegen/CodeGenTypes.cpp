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

    case Ast::AST_DomainType:
        return lowerType(cast<DomainType>(type));

    case Ast::AST_CarrierType:
        return lowerType(cast<CarrierType>(type));

    case Ast::AST_EnumerationType:
        return lowerType(cast<EnumerationType>(type));

    case Ast::AST_TypedefType:
        return lowerType(cast<TypedefType>(type));

    case Ast::AST_FunctionType:
    case Ast::AST_ProcedureType:
        return lowerType(cast<SubroutineType>(type));
    }
}

const llvm::Type *CodeGenTypes::lowerType(DomainType *type)
{
    // FIXME: Lower all domain types to a generic i8*. Perhaps in the future we
    // would like to lower the carrier type instead.
    return CG.getPointerType(llvm::Type::Int8Ty);
}

const llvm::Type *CodeGenTypes::lowerType(CarrierType *type)
{
    return lowerType(type->getRepresentationType());
}

const llvm::IntegerType *CodeGenTypes::lowerType(EnumerationType *type)
{
    // Enumeration types are lowered to an integer type with sufficient capacity
    // to hold each element of the enumeration.
    EnumerationDecl *decl = type->getEnumerationDecl();
    unsigned numBits = llvm::Log2_32_Ceil(decl->getNumLiterals());

    return getTypeForWidth(numBits);
}

const llvm::FunctionType *CodeGenTypes::lowerType(const SubroutineType *type)
{
    std::vector<const llvm::Type*> args;
    const llvm::FunctionType *result;

    // Emit the implicit first "%" argument.
    args.push_back(CG.getRuntime().getType<CommaRT::CRT_DomainInstance>());

    for (unsigned i = 0; i < type->getArity(); ++i) {
        const llvm::Type *argTy = lowerType(type->getArgType(i));

        // If the argument mode is "out" or "in out", make the argument a
        // pointer-to type.
        PM::ParameterMode mode = type->getParameterMode(i);
        if (mode == PM::MODE_OUT or mode == PM::MODE_IN_OUT)
            argTy = CG.getPointerType(argTy);

        args.push_back(argTy);
    }

    if (const FunctionType *ftype = dyn_cast<FunctionType>(type))
        result = llvm::FunctionType::get(lowerType(ftype->getReturnType()), args, false);
    else
        result = llvm::FunctionType::get(llvm::Type::VoidTy, args, false);

    return result;
}

const llvm::IntegerType *CodeGenTypes::lowerType(const TypedefType *type)
{
    // Currently, all TypedefType's are Integer types.
    const IntegerType *baseType = cast<IntegerType>(type->getBaseType());
    return lowerType(baseType);
}

const llvm::IntegerType *CodeGenTypes::lowerType(const IntegerType *type)
{
    return getTypeForWidth(type->getBitWidth());
}

const llvm::IntegerType *CodeGenTypes::getTypeForWidth(unsigned numBits)
{
    // Promote the bit width to a power of two.
    if (numBits <= 1)
        return llvm::Type::Int1Ty;
    if (numBits <= 8)
        return llvm::Type::Int8Ty;
    else if (numBits <= 16)
        return llvm::Type::Int16Ty;
    else if (numBits <= 32)
        return llvm::Type::Int32Ty;
    else if (numBits <= 64)
        return llvm::Type::Int64Ty;
    else {
        // FIXME: This should be a fatal error, not an assert.
        assert(false && "Enumeration type too large to codegen!");
        return 0;
    }
}
