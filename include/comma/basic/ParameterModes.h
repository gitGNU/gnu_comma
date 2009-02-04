//===-- basic/ParameterModes.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// Several components of the compiler need a way to specify the mode of a
// subroutine parameter.  Since all modules of the system can depend on the
// basic library, we specify the following enumeration so that consistent names
// are available to all subsystems.
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_PARAMETERMODES_HDR_GUARD
#define COMMA_BASIC_PARAMETERMODES_HDR_GUARD

namespace comma {

/// Modes associated with the formal parameters of a subroutine.  MODE_DEFAULT
/// is equivalent to MODE_IN, but is used to indicate that the mode was
/// implicitly associated with the parameter, rather than explicitly by the
/// programmer.
enum ParameterMode {
    MODE_DEFAULT,
    MODE_IN,
    MODE_OUT,
    MODE_IN_OUT
};

} // End comma namespace.

#endif
