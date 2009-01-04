//===-- basic/Location.h -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_LOCATION_HDR_GUARD
#define COMMA_BASIC_LOCATION_HDR_GUARD

#include <string>

namespace comma {

class Location {

public:
    Location() : offset(0) { }
    Location(unsigned offset) : offset(offset) { }

    unsigned getOffset() const { return offset; }

    operator unsigned() const { return offset; }

private:
    unsigned offset;
};

class SourceLocation {

public:
    SourceLocation(unsigned line, unsigned column, const std::string &identity)
        : line(line), column(column), identity(identity) { }

    unsigned getLine() const { return line; }
    unsigned getColumn() const { return column; }
    const std::string &getIdentity() const { return identity; }

private:
    unsigned line;
    unsigned column;
    std::string identity;
};

} // End comma namespace.

#endif
