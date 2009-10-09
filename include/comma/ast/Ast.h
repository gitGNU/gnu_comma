//===-- ast/Ast.h --------------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// This file provides the full set of declarations which define the Comma Ast
// hierarchy.  However, note that each of the headers pulled in here are
// #include'able independently.  Thus, if you only require a handfull of the
// definitions provided here, it is best to select the particular headers you
// need and #include them directly.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_AST_AST_HDR_GUARD
#define COMMA_AST_AST_HDR_GUARD

#include "comma/ast/AstBase.h"
#include "comma/ast/AttribExpr.h"
#include "comma/ast/Cunit.h"
#include "comma/ast/Decl.h"
#include "comma/ast/Expr.h"
#include "comma/ast/Stmt.h"
#include "comma/ast/Type.h"

#endif
