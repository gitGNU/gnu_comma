//===-- basic/IdentifierInfo.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009 Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_IDENTIFIERINFO_HDR_GUARD
#define COMMA_BASIC_IDENTIFIERINFO_HDR_GUARD

#include "comma/basic/Attributes.h"

#include "llvm/ADT/StringMap.h"

#include <cassert>

namespace comma {

/// \class IdentifierInfo
/// \brief Associates information with unique strings in the system.
///
/// The IdentifierInfo class is quite fundamental.  These objects provide access
/// to unique (memoized) strings, and to associate arbitrary metadata with each
/// distinct string.  The metadata facility is used to provide O(1) lookup of
/// information knowing only the name of the object of interest.  For example,
/// this facility is used during semantic analysis to implement an efficient
/// symbol table.
///
/// IdentifierInfo's are always allocated with respect to a particular
/// IdentifierPool.  One never constructs an IdentifierInfo explicitly by hand.
/// Allocations are performed by calling one of the
/// IdentifierPool::getIdentifierInfo methods.
///
/// \see IdentifierPool
class IdentifierInfo {

public:
    IdentifierInfo()
        : attributeID(attrib::UNKNOWN_ATTRIBUTE),
          metadata(0) { }

    /// Obtains the unique null terminated string associated with this
    /// identifier.
    const char *getString() const {
        return llvm::StringMapEntry<IdentifierInfo>::
            GetStringMapEntryFromValue(*this).getKeyData();
    }

    /// Returns the metadata associated with this identifier cast to the
    /// supplied type.
    template<typename T>
    T* getMetadata() const { return static_cast<T*>(metadata); }

    /// Associates this IdentifierInfo object with the supplied metadata.  The
    /// ownership of this pointer remains with the caller.
    ///
    /// \param mdata  The metadata to associate with this identifier.
    void setMetadata(void *mdata) { metadata = mdata; }

    /// Returns true if the metadata has been set to a non-null pointer.
    bool hasMetadata() const { return metadata != 0; }

    /// Sets the attribute ID for this identifier.
    void setAttributeID(attrib::AttributeID ID) { attributeID = ID; }

    /// Returns the attribute ID for this identifier.
    attrib::AttributeID getAttributeID() const { return attributeID; }

private:
    attrib::AttributeID attributeID : 8; ///< Attribute ID of this identifier.
    void *metadata;
};

} // End comma namespace

#endif
