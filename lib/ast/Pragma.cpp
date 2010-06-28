//===-- ast/Pragma.cpp ---------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/ast/Expr.h"
#include "comma/ast/Pragma.h"

#include <cstring>

using namespace comma;

PragmaImport::PragmaImport(Location loc, Convention convention,
                           IdentifierInfo *entity, Expr *externalName)
    : Pragma(pragma::Import, loc),
      convention(convention),
      entity(entity),
      externalNameExpr(externalName)
{
    externalName->staticStringValue(this->externalName);
}

PragmaImport::Convention PragmaImport::getConventionID(llvm::StringRef &ref)
{
    // There is only one convention supported ATM: C.
    if (ref.size() == 1 && ref[0] == 'c')
        return C;
    return UNKNOWN_CONVENTION;
}
