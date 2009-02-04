//===-- typecheck/TypeEqual.h --------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
//  This file provides several predicates for type equality needed by the type
//  checking code.
//
//===----------------------------------------------------------------------===//

#ifndef TYPECHECK_TYPEEQUAL_HDR_GUARD
#define TYPECHECK_TYPEEQUAL_HDR_GUARD

#include "comma/ast/Type.h"
#include "comma/ast/Decl.h"
#include "comma/ast/AstRewriter.h"

namespace comma {

// compareTypesUsingRewrites: the following functions determine equality between
// two types using the supplied rewrite rules as a context for evaluation.
bool compareTypesUsingRewrites(const AstRewriter &rewrites,
                               SignatureType     *typeX,
                               SignatureType     *typeY);

bool compareTypesUsingRewrites(const AstRewriter &rewrites,
                               DomainType        *typeX,
                               DomainType        *typeY);

bool compareTypesUsingRewrites(const AstRewriter &rewrites,
                               SubroutineType    *typeX,
                               SubroutineType    *typeY);

bool compareTypesUsingRewrites(const AstRewriter &rewrites,
                               FunctionType      *typeX,
                               FunctionType      *typeY);

bool compareTypesUsingRewrites(const AstRewriter &rewrites,
                               ProcedureType     *typeX,
                               ProcedureType     *typeY);



} // End comma namespace.

#endif
