//===-- basic/HashUtils.cpp ----------------------------------- -*- C++ -*-===//
//
// The hash functions in this file are derived from the Fowler/Noll/Vo (FVN)
// Hash algorithms (http://www.isthe.com/chongo/tech/comp/fnv).  The original
// implementations have been placed in the public domain.  In keeping with the
// FVN authors lead, this file is hereby placed in the public domain, and is not
// under copywrite.
//
//===----------------------------------------------------------------------===//

#include "comma/basic/HashUtils.h"
#include <string>
#include <cstring>

using namespace comma;

#define HASH_BASIS 2166136261
#define HASH_PRIME 16777619

uint32_t comma::hashString(const char *x)
{
    uint32_t hval = HASH_BASIS;

    while (*x) {
        hval *= HASH_PRIME;
        hval ^= uint32_t(*x++);
    }
    return hval;
}

uint32_t comma::hashData(const void *ptr, size_t size)
{
    const unsigned char *cursor = static_cast<const unsigned char *>(ptr);
    const unsigned char *end    = cursor + size;
    uint32_t hval               = HASH_BASIS;

    while (cursor != end) {
        hval *= HASH_PRIME;
        hval ^= uint32_t(*cursor++);
    }
    return hval;
}

size_t StrHash::operator() (const char *x) const
{
    return hashString(x);
}

bool StrEqual::operator() (const char *x, const char *y) const
{
    return strcmp(x, y) == 0;
}
