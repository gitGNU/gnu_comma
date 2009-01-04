//===-- basic/IdentifierInfo.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_IDENTIFIERINFO_HDR_GUARD
#define COMMA_BASIC_IDENTIFIERINFO_HDR_GUARD

#include "llvm/ADT/StringMap.h"
#include <cassert>

namespace comma {

// All identifiers are interned (uniqued) when initially lexed, and are
// associated with an IdentiferInfo object.  The IdentifierInfo objects
// themselves provide fast access to:
//
//   - The string representation of the identifier.

//   - Arbitrary metadata associated with a given identifier: This is used
//     primarily during semantic analysis to implement fast symbol table
//     lookups.
class IdentifierInfo {
public:
    IdentifierInfo() : metadata(0) { }

    // Returns a unique null terminated string associated with this
    // identifier.
    const char *getString() const {
        return llvm::StringMapEntry<IdentifierInfo>::
            GetStringMapEntryFromValue(*this).getKeyData();
    }

    // Returns the metadata cast to the supplied type.
    template<typename T>
    T* getMetadata() const { return static_cast<T*>(metadata); }

    // Associates this Identifier_Info objcect with the supplied metadata.  The
    // ownership of this pointer remains with the caller.
    void setMetadata(void *mdata) { metadata = mdata; }

    // Returns true if the metadata has been set to a non-null pointer.
    bool hasMetadata() const { return metadata != 0; }

private:
    void *metadata;
};

} // End comma namespace

#endif
