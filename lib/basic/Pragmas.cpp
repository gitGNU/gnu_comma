//===-- basic/Pragmas.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/Pragmas.h"

#include <cstring>

using namespace comma::pragma;

namespace {

static const char *pragmaNames[] = {
    "Assert",
    "Import"
};

} // end anonymous namespace.

const char *comma::pragma::getPragmaString(PragmaID ID)
{
    return pragmaNames[ID - FIRST_PRAGMA];
}

PragmaID comma::pragma::getPragmaID(const char *start, const char *end)
{
    size_t len = end - start;

    for (unsigned cursor = FIRST_PRAGMA; cursor <= LAST_PRAGMA; ++cursor) {
        PragmaID ID = static_cast<PragmaID>(cursor);
        const char *name = getPragmaString(ID);
        if (::strlen(name) == len && strncmp(name, start, len) == 0)
            return ID;
    }
    return UNKNOWN_PRAGMA;
}
