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

class TextProvider;

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
/// This class encapsulates explicit line/column information associated with a
/// particular TextProvider.
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
    /// \param line The line coordinate.
    ///
    /// \param column The column coordinate.
    ///
    /// \param provider The TextProvider object this SourceLocation describes.
    SourceLocation(unsigned line, unsigned column, const TextProvider *provider)
        : line(line), column(column), provider(provider) { }

    /// \brief Constructs an uninitialized SourceLocation object.
    SourceLocation() : line(0), column(0), provider(0) { }

    /// \brief Accesses the line coordinate.
    unsigned getLine() const { return line; }

    /// \brief Accesses the column coordinate.
    unsigned getColumn() const { return column; }

    /// \breif Accesses the associcated TextProvider.
    const TextProvider *getTextProvider() const { return provider; }

private:
    unsigned line;
    unsigned column;
    const TextProvider *provider;
};

} // End comma namespace.

#endif
