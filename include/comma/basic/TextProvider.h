//===-- basic/TextProvider.h ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_TEXTPROVIDER_HDR_GUARD
#define COMMA_BASIC_TEXTPROVIDER_HDR_GUARD

#include "comma/basic/Location.h"
#include "llvm/System/Path.h"
#include "llvm/Support/MemoryBuffer.h"
#include <string>
#include <vector>

namespace comma {

/// \class TextIterator
/// \brief Iterates over the character data associated with a TextProvider.
///
/// Iteration over a character buffer can be accomplished efficiently using
/// nothing more than a pointer to char.  However, in the future, we may which
/// to accommodate Unicode source files which require more sophisticated
/// mechanisms to access the character data.  Currently this class can be, in
/// principle, optimized down to a simple pointer to char, but may evolve in
/// time to perform a more elaborate service.
class TextIterator {
public:

    TextIterator(const TextIterator &ti)
        : cursor(ti.cursor)
        { }

    TextIterator &operator=(const TextIterator &ti) {
        cursor = ti.cursor;
        return *this;
    }

    bool operator==(const TextIterator &ti) const {
        return cursor == ti.cursor;
    }

    bool operator!=(const TextIterator &ti) const {
        return cursor != ti.cursor;
    }

    TextIterator &operator++() {
        ++cursor;
        return *this;
    }

    TextIterator operator++(int) {
        TextIterator prev(*this);
        ++cursor;
        return prev;
    }

    TextIterator &operator--() {
        --cursor;
        return *this;
    }

    TextIterator operator--(int) {
        TextIterator copy(*this);
        --cursor;
        return copy;
    }

    unsigned operator*() const {
        return *cursor;
    }

    const char *operator&() const {
        return cursor;
    }

private:
    friend class TextProvider;

    /// Provides an initial iterator over the given character array.
    TextIterator(const char *c) : cursor(c) { }

    const char *cursor;
};

/// \class TextProvider
/// \brief Encapsulates a source of characters.
///
/// The principle purpose of this class is to manage a source of character
/// data.  The source of the data may be a file or a raw buffer.  Mechanisms are
/// provided to iterate over the underlying characters, retrieve location
/// information in the form of line/column coordinates, and to extract ranges of
/// text.
///
/// Currently, lines are numbered beginning at 1 and columns are numbered
/// beginning at 0.  This is the conventional system used by Emacs.  Adding
/// additional indexing strategies should be straight forward to implement.
///
/// \todo Provide a set of alternative line/column indexing strategies.
class TextProvider {

public:
    /// \brief Construct a TextProvider over the given file.
    ///
    /// Initializes a TextProvider to manage the contents of the given file.
    /// The provided path must name a readable text file, or if the path
    /// specifies a file name "-", then read from all of stdin instead.  If the
    /// path is invalid, this constructor will simply call abort.
    ///
    /// \param path The file used to back this TextProvider.
    TextProvider(const llvm::sys::Path& path);

    /// \brief Construct a TextProvider over the given buffer.
    ///
    /// Initializes a TextProvider to manage the contents of the given region of
    /// memory.  The contents of the buffer are copied -- the TextProvider does
    /// not take ownership of the memory region.
    ///
    /// \param buffer Pointer to the start of the memory region.
    ///
    /// \param size The size in bytes of the memory region.
    TextProvider(const char *buffer, size_t size);

    /// \brief Construct a TextProvider over the given string.  The contents of
    /// the string are copied.
    ///
    /// \param string The string used to back this TextProvider.
    TextProvider(const std::string &string);

    ~TextProvider();

    /// \brief Returns the Location object corresponding to the position of the
    /// supplied TextIterator.
    ///
    /// \param ti The iterator we need a location for.
    ///
    /// \return A Location object corresponding to the supplied iterator.
    ///
    /// \see Location
    Location getLocation(const TextIterator &ti) const;

    /// \brief Returns a SourceLocation object corresponding to the position of
    /// the supplied iterator.
    SourceLocation getSourceLocation(const TextIterator &ti) const;

    /// \brief Returns a SourceLocation object corresponding to the given raw
    /// Location.
    SourceLocation getSourceLocation(const Location loc) const;

    /// \brief Returns the line number associated with the given Location
    /// object.
    unsigned getLine(Location) const;

    /// \brief Returns the column number associated with the given Location.
    unsigned getColumn(Location) const;

    /// \brief Returns the line number associated with the given TextIterator.
    unsigned getLine(const TextIterator &ti) const {
        return getLine(getLocation(ti));
    }

    /// \brief Returns the column number associated with the given TextIterator.
    unsigned getColumn(const TextIterator &ti) const {
        return getColumn(getLocation(ti));
    }

    /// \brief Returns an iterator corresponding to the start of this
    /// TextProviders character data.
    TextIterator begin() const;

    /// \brief Returns a sentinel iterator.
    TextIterator end() const;

    /// \brief Provides access to a range of text.
    ///
    /// Returns a string corresponding to the range of text starting at location
    /// \a start and ends at location \a end.  The range of text returned
    /// includes both of its end points.
    ///
    /// \param start The location of the first character to be extracted.
    ///
    /// \param end The location of the last character to be extracted.
    std::string extract(Location start, Location end) const;

    /// \brief Provides access to a range of text.
    ///
    /// Returns a string corresponding the the range of text starting at the
    /// position of \a iter and ending just before the position of \a endIter.
    ///
    /// \param iter The position of the first character to be extracted.
    ///
    /// \param endIter The position of one past the last character to be
    /// extracted.
    std::string extract(const TextIterator &iter,
                        const TextIterator &endIter) const;

    /// \brief Extracts a range of text into a buffer.
    ///
    /// Places the range of text delimited by the given pair of iterators into
    /// the given buffer.
    ///
    /// \param iter The position of the first character to be extracted.
    ///
    /// \param endIter The position of one past the last character to be
    /// extracted.
    ///
    /// \param buffer A pointer to a region of memory to be filled with
    /// character data.  If this is the NULL pointer, then no data is written
    /// and this function returns the number of characters which would have been
    /// written assuming a large enough buffer was provided.
    ///
    /// \param size The number of characters which the supplied buffer can
    /// accommodate.  If the size is smaller than the number of characters which
    /// the iterators delimit, then only \a size characters are written.
    ///
    /// \return The number of characters written, or if \a buffer is NULL, the
    /// number of characters that would have been written if the supplied buffer
    /// was large enough.
    unsigned extract(const TextIterator &iter,
                     const TextIterator &endIter,
                     char *buffer, size_t size) const;


private:
    friend class TextIterator;

    // Disallow copy and assignment.
    TextProvider(const TextProvider&);
    TextProvider &operator=(const TextProvider&);

    /// Returns the offset into the underlying character buffer given a pointer
    /// into the buffer.
    unsigned indexOf(const char *ptr) const {
        return ptr - buffer;
    }

    /// Returns the indexes of the start and end of the line of text which
    /// contains the given location.
    std::pair<unsigned, unsigned> getLineOf(Location loc) const;

    /// A string identifying this TextProvider.  This is typically a file name
    /// or the name of an input stream.
    std::string identity;

    /// The underlying MemoryBuffer.
    llvm::MemoryBuffer *memBuffer;

    /// A pointer to \a memBuffer's data.
    const char *buffer;

    /// A mapping from lines to indexes into the memory buffer.  For example,
    /// lines[i] yields the offset into the memory buffer which starts line \p
    /// i.  Similarly, lines[i + 1] yields the offset into the memory buffer
    /// which starts line \p{i + 1}, it also provides the offset of the
    /// character immediately following the line break character which ends the
    /// \p i'th line.  This vector is always initialized so that lines[0] == 0.
    mutable std::vector<unsigned> lines;

    /// Ensure that lines[0] == 0.
    void initializeLinevec();

    /// We maintain a auxiliary variable maxLineIndex and keep it so that
    /// maxLineIndex >= lines[lines.length() - 1].  This allows us to easily
    /// check if a given offset is within the range of text covered by the line
    /// vector.
    mutable unsigned maxLineIndex;
};

} // End comma namespace.

#endif



