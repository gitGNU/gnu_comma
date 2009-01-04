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
#include <utility>
#include <iosfwd>

namespace comma {

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

    // Provides initial iterator over the given character array.
    TextIterator(const char *c) : cursor(c) { }

    const char *cursor;
};

class TextProvider {
public:

    TextProvider(const llvm::sys::Path& path);
    TextProvider(const char *buffer, size_t size);
    TextProvider(const std::string &string);
    ~TextProvider();

    Location getLocation(const TextIterator &ti) const;

    SourceLocation getSourceLocation(const TextIterator &ti) const;
    SourceLocation getSourceLocation(const Location loc) const;

    unsigned getLine(Location) const;
    unsigned getColumn(Location) const;

    unsigned getLine(const TextIterator &ti) const {
        return getLine(getLocation(ti));
    }

    unsigned getColumn(const TextIterator &ti) const {
        return getColumn(getLocation(ti));
    }

    TextIterator begin() const;
    TextIterator end() const;

    std::string extract(unsigned x, unsigned y) const;

    std::string extract(const TextIterator &iter,
                        const TextIterator &endIter) const;

    void extract(const TextIterator &iter,
                 const TextIterator &endIter, std::string &dst) const;

    unsigned extract(const TextIterator &iter,
                     const TextIterator &endIter,
                     char *buffer, size_t size) const;


    std::pair<unsigned, unsigned> getLineOf(Location loc) const;

private:
    friend class TextIterator;

    // Disallow copy and assignment.
    TextProvider(const TextProvider&);
    TextProvider &operator=(const TextProvider&);

    unsigned indexOf(const char *ptr) const {
        return ptr - buffer;
    }

    // A string identifying this TextProvider.  This is typically a file name or
    // the name of an input stream.
    std::string identity;

    // The underlying MemoryBuffer and a pointer to the start of the buffers
    // data.
    llvm::MemoryBuffer *memBuffer;
    const char *buffer;

    // Set of index -> line mappings.  For i < lines.length() -1, the range
    // [lines[i] ... lines[i + 1]) denotes the set of native indexes which
    // belong to line i (zero indexed).  The vector is always initialized so
    // that lines[0] == 0.
    void initializeLinevec();

    mutable std::vector<unsigned> lines;

    // We maintain a auxiliary variable maxLineIndex and keep it so that
    // maxLineIndex >= lines[lines.lenght() - 1].
    mutable unsigned maxLineIndex;
};

std::ostream &printLocation(std::ostream &s, Location loc, TextProvider &tp);

} // End comma namespace

#endif



