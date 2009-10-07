//===-- basic/Attributes.h ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
/// \file
///
/// \brief Defines attributes and various related utility routines.
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_ATTRIBUTES_HDR_GUARD
#define COMMA_BASIC_ATTRIBUTES_HDR_GUARD

#include "llvm/ADT/StringRef.h"

namespace comma {

class IdentifierPool;

namespace attrib {

/// The following enumeration lists all of Comma's attributes.
///
/// UNKNOWN_ATTRIBUTE is a special marker which does not map to any attribute.
enum AttributeID {
    UNKNOWN_ATTRIBUTE,
    First,
    Last,

    // Markers delimiting special attribute subgroups.
    FIRST_ATTRIB = First,
    LAST_ATTRIB = Last
};

/// Marks all of the identifiers in the given pool with their associated
/// attributeID.
void markAttributeIdentifiers(IdentifierPool &idPool);

/// Returns the attribute id for the string delimited by the pointers \p start
/// and \p end, or UNKNOWN_ATTRIBUTE if the string does not name an attribute.
AttributeID getAttributeID(const char *start, const char *end);

/// Returns the attribute id for the given string, or UNKNOWN_ATTRIBUTE if the
/// string does not name an attribute.
inline AttributeID getAttributeID(llvm::StringRef &name) {
    return getAttributeID(name.begin(), name.end());
}

} // end attrib namespace.

} // end comma namespace.

#endif
