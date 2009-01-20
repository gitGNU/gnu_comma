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

/// \class Location
/// \brief Provides a light-weight indicator of a position in source code.
///
/// Location objects are designed to be used in tandem with a particular
/// instance of a TextProvider.  They function simply as a key which a
/// TextProvider can interpret to provide line and column information.  More
/// precisely, Locations are associated with a raw unsigned value known as its
/// offset (since a TextProvider uses it as an index into internal tables to
/// determine its associated line/column info).  An offset of zero is reserved
/// to indicate an invalid or non-existent location.
///
/// Location objects are typically created via a call to
/// TextProvider::getLocation.
///
/// \see SourceLocation
/// \see TextProvider
class Location {

public:
    /// \brief Constructs an invalid Location object.
    Location() : offset(0) { }

    /// \brief Constructs a Location with the given offset.
    Location(unsigned offset) : offset(offset) { }

    /// \brief Returns true if this Location is invalid.
    ///
    /// An invalid location does not correspond to any position in a source
    /// file).
    bool isInvalid() const { return offset == 0; }

    /// \brief Returns true if this Location is valid.
    bool isValid() const { return !isInvalid(); }

    /// \brief Returns the offset associated with this Location.
    unsigned getOffset() const { return offset; }

    /// \brief Converts this Location to an unsigned integer.
    operator unsigned() const { return offset; }

private:
    unsigned offset;
};

/// \class SourceLocation
/// \brief Provides explicit line/column information.
///
/// This class encapsulates explicit line.column information and provides an
/// identifying string.  Typically the string names the source file which the
/// line/column coordinates correspond, or a name describing the source of some
/// character data.
///
/// SourceLocation objects are typically created via a call to
/// TextProvider::getSourceLocation.
///
/// \see Location
/// \see TextProvider
class SourceLocation {

public:
    /// \brief Constructs a SourceLocation object.
    ///
    /// \param  line  The line coordinate.
    ///
    /// \param  column  The column coordinate.
    ///
    /// \param  identity  A string indicating the source file or input stream
    /// associated with this object.
    SourceLocation(unsigned line, unsigned column, const std::string &identity)
        : line(line), column(column), identity(identity) { }

    /// \brief Accesses the line coordinate.
    unsigned getLine() const { return line; }

    /// \brief Accesses the column coordinate.
    unsigned getColumn() const { return column; }

    /// \brief Accesses the associated identity of this SourceLocation.
    const std::string &getIdentity() const { return identity; }

private:
    unsigned line;
    unsigned column;
    std::string identity;
};

} // End comma namespace.

#endif
