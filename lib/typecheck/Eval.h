//===-- typecheck/Eval.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief This file provides subroutines which implement compile-time
/// evaluation of static expressions.
//===----------------------------------------------------------------------===//

#ifndef COMMA_TYPECHECK_EVAL_HDR_GUARD
#define COMMA_TYPECHECK_EVAL_HDR_GUARD

namespace llvm {

class APInt;

} // end llvm namespace.

namespace comma {

class Expr;

namespace eval {

/// Attempts to evaluate \p expr as a static integer expression.
///
/// \param expr The expression to evaluate.
///
/// \param result If \p expr is a static expression, \p result is set to the
/// computed value.
///
/// \return True if \p expr is static and \p result was set.  False otherwise.
bool staticIntegerValue(Expr *expr, llvm::APInt &result);

} // end comma::eval namespace.

} // end comma namespace.

#endif
