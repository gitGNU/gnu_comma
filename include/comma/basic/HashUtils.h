//===-- basic/HashUtils.h ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_HASHUTILS_HDR_GUARD
#define COMMA_BASIC_HASHUTILS_HDR_GUARD

#include <cstddef>
#include "llvm/Support/DataTypes.h"

namespace comma {

// The following functions provide hash code generation over a variety of basic
// datatypes.

// Returns a hash for the given null terminated string.
uint32_t hashString(const char *x);

// Returns a hash for the data pointed to be ptr, size bytes in length.
uint32_t hashData(const void *ptr, size_t size);

// When using the standard library unordered_map or unordered_set containers
// with C strings as keys the following structures provide functional objects
// suitable for use as template parameters.
struct StrHash {
    size_t operator() (const char *x) const;
};

struct StrEqual {
    bool operator() (const char *x, const char *y) const;
};

} // End comma namespace

#endif
