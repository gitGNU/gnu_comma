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

Node TypeCheck::acceptProcedureCall(IdentifierInfo  *name,
                                    Location         loc,
                                    Node            *args,
                                    unsigned         numArgs)
{
    return acceptSubroutineCall(name, loc, args, numArgs, false);
}
