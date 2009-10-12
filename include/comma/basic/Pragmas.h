//===-- basic/Pragmas.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief General encodings and support utilities for the language defined
/// pragmas.
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_PRAGMAS_HDR_GUARD
#define COMMA_BASIC_PRAGMAS_HDR_GUARD

#include "llvm/ADT/StringRef.h"

namespace comma {

namespace pragma {

/// Enumeration listing each supported pragma.  The special marker
/// UNKNOWN_PRAGMA does not map directly to any pragma.
enum PragmaID {
    UNKNOWN_PRAGMA,
    Assert,
    Import,

    // Delimiters marking the set of proper pragma values.
    FIRST_PRAGMA = Assert,
    LAST_PRAGMA = Import
};

/// Returns the pragma id for the string delimited by the pointers \p start and
/// \p end, or UNKNOWN_PRAGMA if the string does not name a pragma.
PragmaID getPragmaID(const char *start, const char *end);

/// Returns the pragma id for the given string, or UNKNOWN_PRAGMA if the
/// string does not name a pragma.
inline PragmaID getPragmaID(llvm::StringRef &name) {
    return getPragmaID(name.begin(), name.end());
}

/// Returns a null terminated string naming the given pragma.  UNKNOWN_PRAGMA is
/// not a valid ID in this case.
const char *getPragmaString(PragmaID ID);

} // end pragma namespace.

} // end comma namespace.

#endif
