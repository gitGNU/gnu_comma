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

// FIXME:  Imports are not statements, they are "clauses".
void TypeCheck::acceptImportStatement(Node importedNode, Location loc)
{
    Type       *type = cast_node<Type>(importedNode);
    DomainType *domain;

    if (CarrierType *carrier = dyn_cast<CarrierType>(type))
        domain = dyn_cast<DomainType>(carrier->getRepresentationType());
    else
        domain = dyn_cast<DomainType>(type);

    if (!domain) {
        report(loc, diag::IMPORT_FROM_NON_DOMAIN);
        return;
    }

    scope.addImport(domain);
}


Node TypeCheck::acceptProcedureCall(IdentifierInfo  *name,
                                    Location         loc,
                                    Node            *args,
                                    unsigned         numArgs)
{
    return acceptSubroutineCall(name, loc, args, numArgs, false);
}
