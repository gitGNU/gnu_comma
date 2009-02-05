//===-- typecheck/CheckStmt.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Decl.h"
#include "comma/ast/Type.h"
#include "comma/typecheck/TypeCheck.h"
#include "llvm/Support/Casting.h"

using namespace comma;
using llvm::dyn_cast;
using llvm::cast;
using llvm::isa;


void TypeCheck::acceptImportStatement(Node importedNode, Location loc)
{
    ModelType *model = lift<ModelType>(importedNode);
    DomainType *domain;

    assert(model && "Bad node kind!");

    domain = dyn_cast<DomainType>(model);
    if (!domain) {
        report(loc, diag::IMPORT_FROM_NON_DOMAIN) << model->getString();
        return;
    }
}

