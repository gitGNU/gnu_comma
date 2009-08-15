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

AstResource::AstResource(TextProvider   &txtProvider,
                         IdentifierPool &idPool)
    : txtProvider(txtProvider),
      idPool(idPool) { }

IntegerType *AstResource::getIntegerType(const llvm::APInt &low,
                                         const llvm::APInt &high)
{
    llvm::FoldingSetNodeID ID;
    IntegerType::Profile(ID, low, high);

    void *pos = 0;
    if (IntegerType *IT = integerTypes.FindNodeOrInsertPos(ID, pos))
        return IT;

    IntegerType *res = new IntegerType(low, high);
    integerTypes.InsertNode(res, pos);
    return res;
}
