//===-- ast/AstResource.cpp ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/AstResource.h"
#include "comma/ast/Type.h"
#include "comma/ast/Decl.h"

using namespace comma;

AstResource::AstResource(TextProvider &txtProvider, IdentifierPool &idPool)
    : txtProvider(txtProvider),
      idPool(idPool) { }


/// Returns a uniqued FunctionType.
FunctionType *AstResource::getFunctionType(Type **argTypes, unsigned numArgs,
                                           Type *returnType)
{
    llvm::FoldingSetNodeID ID;
    FunctionType::Profile(ID, argTypes, numArgs, returnType);

    void *pos = 0;
    if (FunctionType *uniqued = functionTypes.FindNodeOrInsertPos(ID, pos))
        return uniqued;

    FunctionType *res = new FunctionType(argTypes, numArgs, returnType);
    functionTypes.InsertNode(res, pos);
    return res;
}

/// Returns a uniqued ProcedureType.
ProcedureType *AstResource::getProcedureType(Type **argTypes, unsigned numArgs)
{
    llvm::FoldingSetNodeID ID;
    ProcedureType::Profile(ID, argTypes, numArgs);

    void *pos = 0;
    if (ProcedureType *uniqued = procedureTypes.FindNodeOrInsertPos(ID, pos))
        return uniqued;

    ProcedureType *res = new ProcedureType(argTypes, numArgs);
    procedureTypes.InsertNode(res, pos);
    return res;
}

IntegerType *AstResource::getIntegerType(IntegerDecl *decl,
                                         const llvm::APInt &low,
                                         const llvm::APInt &high)
{
    IntegerType *res = new IntegerType(decl, low, high);
    integerTypes.push_back(res);
    return res;
}

ArrayType *AstResource::getArrayType(ArrayDecl *decl, unsigned rank,
                                     SubType **indices, Type *component,
                                     bool isConstrained)
{
    ArrayType *res;
    res = new ArrayType(decl, rank, indices, component, isConstrained);
    arrayTypes.push_back(res);
    return res;
}

