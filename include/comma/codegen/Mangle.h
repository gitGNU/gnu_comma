//===-- codegen/Mangle.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Provides routines which generate the mangled names of Comma entities.
//===----------------------------------------------------------------------===//

#ifndef COMMA_CODEGEN_MANGLE_HDR_GUARD
#define COMMA_CODEGEN_MANGLE_HDR_GUARD

#include "comma/ast/AstBase.h"

namespace comma {

namespace mangle {

/// \brief Returns the name of the given subroutine as it should appear in LLVM
/// IR.
///
/// The conventions followed by Comma model those of Ada.  In particular, a
/// subroutines link name is similar to its fully qualified name, except that
/// the dot is replaced by an underscore, and overloaded names are identified
/// using a postfix number.  For example, the Comma name "D.Foo" is translated
/// into "D__Foo" and subsequent overloads are translated into "D__Foo__1",
/// "D__Foo__2", etc.  Operator names like "*" or "+" are given names beginning
/// with a zero followed by a spelled out alternative.  For example, "*"
/// translates into "0multiply" and "+" translates into "0plus" (with
/// appropriate qualification prefix and overload suffix).
std::string getLinkName(const SubroutineDecl *sr);

std::string getLinkName(const CapsuleInstance *instance,
                        const SubroutineDecl *sr);

/// \brief Returns the name of the given Domoid as it should appear in LLVM
/// IR.
std::string getLinkName(const Domoid *domoid);

/// \brief Returns the name of the given instance as it should appear in LLVM
/// IR.
std::string getLinkName(const CapsuleInstance *instance);

/// \brief Returns the name of the given exception declaration as it should
/// appear in LLVM IR.
///
/// \note This routine can only be called on user defined exceptions.  The names
/// of system exceptions are defined by the runtime library and accessed thru
/// CommaRT exclusively.
std::string getLinkName(const ExceptionDecl *exception);

} // end mangle namespace.

} // end comma namespace.

#endif
