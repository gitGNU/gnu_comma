//===-- basic/IdentifierPool.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_IDENTIFIERPOOL_HDR_GUARD
#define COMMA_BASIC_IDENTIFIERPOOL_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "llvm/ADT/StringMap.h"

namespace comma {

/// \class IdentifierPool
/// \brief Collections of IdentifierInfo objects.
///
/// This class is much like a hash table mapping strings to unique
/// IdentifierInfo objects.  The interface to this class is quite sparse, as its
/// primary roll is to serve as an allocator which persists throughout the life
/// of the program.
///
/// \see IdentifierInfo
class IdentifierPool {

    typedef llvm::StringMap<IdentifierInfo> PoolType;
    PoolType pool;

public:
    /// \brief Returns an IdentifierInfo object associated with the given
    /// string.
    ///
    /// If no IdentifierInfo object is associated with the supplied string, the
    /// data is copied and a new IdentifierInfo node created.  The string need
    /// not be null terminated (the copy will be terminated).
    ///
    /// \param  name  The string to associate with an IdentifierInfo object.
    /// \param  len   The length of the supplied string.
    ///
    /// \return An interned (unique) IdentifierInfo object associated with \a
    /// name.
    IdentifierInfo &getIdentifierInfo(const char *name, size_t len) {
        return pool.GetOrCreateValue(name, name + len).getValue();
    }

    /// \brief Returns the IdentifierInfo associated with the given null
    /// terminated string.
    ///
    /// \param  name  The string to associate with an IdentifierInfo object.
    ///
    /// \return An interned (unique) IdentifierInfo object associated with \a
    /// name.
    IdentifierInfo &getIdentifierInfo(const char *name) {
        return pool.GetOrCreateValue(name, name + strlen(name)).getValue();
    }

    /// \brief Returns the IdentifierInfo associated with the given std::string.
    ///
    /// \param  name  The string to associate with an IdentifierInfo object.
    ///
    /// \return An interned (unique) IdentifierInfo object associated with \a
    /// name.
    IdentifierInfo &getIdentifierInfo(const std::string& name) {
        // We do not use c_str() here since the we do not require null
        // termination.
        const char *rep = &name[0];
        return getIdentifierInfo(rep, name.size());
    }

    typedef PoolType::const_iterator iterator;

    /// \brief Provides an iterator over the elements of of this pool.
    ///
    /// The objects returned are of type llvm::StringMapEntry -- essentially key
    /// value pairs of string and IdentifierInfo objects.
    iterator begin() const { return pool.begin(); }

    /// \brief Sentinel marking the end of iteration.
    iterator end()   const { return pool.end(); }

    /// Returns the number of IdentifierInfo's managed by this pool.
    unsigned size() const { return pool.size(); }
};

} // End comma namespace

#endif
