//===-- basic/IdentifierPool.h -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_IDENTIFIERPOOL_HDR_GUARD
#define COMMA_BASIC_IDENTIFIERPOOL_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "llvm/ADT/StringMap.h"

namespace comma {

class IdentifierPool {

    typedef llvm::StringMap<IdentifierInfo> PoolType;
    PoolType pool;

    static IdentifierPool spool;

public:
    IdentifierInfo &getIdentifierInfo(const char *name, size_t len) {
        return pool.GetOrCreateValue(name, name +len).getValue();
    }

    // Returns the IdentifierInfo associated with the given null terminated
    // string.
    IdentifierInfo &getIdentifierInfo(const char *name) {
        return pool.GetOrCreateValue(name, name + strlen(name)).getValue();
    }

    IdentifierInfo &getIdentifierInfo(const std::string& name) {
        return getIdentifierInfo(name.c_str());
    }

    static IdentifierInfo &getIdInfo(const char *name) {
        return spool.getIdentifierInfo(name);
    }

    static IdentifierInfo &getIdInfo(const char *name, size_t len) {
        return spool.getIdentifierInfo(name, len);
    }

    // Iterators over the IdentifierInfo entries.
    typedef PoolType::const_iterator iterator;

    iterator begin() const { return pool.begin(); }
    iterator end()   const { return pool.end(); }

    // Returns the number of IdentifierInfos managed by this pool.
    unsigned size() const { return pool.size(); }
};

extern IdentifierPool *identifierPool;

} // End comma namespace

#endif
