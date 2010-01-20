//===-- basic/Attributes.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/Attributes.h"
#include "comma/basic/IdentifierPool.h"

#include <cstring>

using namespace comma::attrib;

namespace {

static const char *attributeNames[] = {
    "First",
    "Pos",
    "Val",
    "Last",
    "Range",
};

} // end anonymous namespace.

const char *comma::attrib::getAttributeString(AttributeID ID)
{
    return attributeNames[ID - FIRST_ATTRIB];
}

void comma::attrib::markAttributeIdentifiers(IdentifierPool &idPool)
{
    for (unsigned cursor = FIRST_ATTRIB; cursor <= LAST_ATTRIB; ++cursor) {
        AttributeID ID = static_cast<AttributeID>(cursor);
        const char *name = getAttributeString(ID);
        IdentifierInfo &idInfo = idPool.getIdentifierInfo(name);
        idInfo.setAttributeID(ID);
    }
}

AttributeID comma::attrib::getAttributeID(const char *start, const char *end)
{
    size_t len = end - start;

    for (unsigned cursor = FIRST_ATTRIB; cursor <= LAST_ATTRIB; ++cursor) {
        AttributeID ID = static_cast<AttributeID>(cursor);
        const char *name = getAttributeString(ID);
        if (::strlen(name) == len && strncmp(name, start, len) == 0)
            return ID;
    }
    return UNKNOWN_ATTRIBUTE;
}
