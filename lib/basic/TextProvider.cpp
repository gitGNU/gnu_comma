//===-- basic/TextProvider.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/TextProvider.h"
#include <ostream>
#include <cstdlib>
#include <cstring>
#include <cassert>

using namespace comma;

TextProvider::TextProvider(const llvm::sys::Path &path)
{
    memBuffer = llvm::MemoryBuffer::getFileOrSTDIN(path.c_str());

    // Perhaps it would be better to simply return an empty TextProvider in this
    // case.
    if (!memBuffer) abort();

    buffer = memBuffer->getBufferStart();

    identity = path.getLast();

    // If we are reading from stdin, update identity to a more meningful string.
    if (identity.compare("-") == 0)
        identity = "<stdin>";

    initializeLinevec();
}

TextProvider::TextProvider(const char *raw, size_t length)
{
    memBuffer = llvm::MemoryBuffer::getMemBufferCopy(raw, raw + length);
    buffer = memBuffer->getBufferStart();
    initializeLinevec();
}

TextProvider::TextProvider(const std::string &str)
{
    const char *start = str.c_str();
    const char *end   = start + str.size();
    memBuffer = llvm::MemoryBuffer::getMemBufferCopy(start, end);
    buffer = memBuffer->getBufferStart();
    initializeLinevec();
}

TextProvider::~TextProvider()
{
    delete memBuffer;
}

Location TextProvider::getLocation(const TextIterator &ti) const
{
    return indexOf(ti.cursor);
}

SourceLocation TextProvider::getSourceLocation(const TextIterator &ti) const
{
    unsigned line   = getLine(ti);
    unsigned column = getColumn(ti);
    return SourceLocation(line, column, this);
}

SourceLocation TextProvider::getSourceLocation(const Location loc) const
{
    unsigned line   = getLine(loc);
    unsigned column = getColumn(loc);
    return SourceLocation(line, column, this);
}

TextIterator TextProvider::begin() const
{
    return TextIterator(buffer);
}

TextIterator TextProvider::end() const
{
    return TextIterator(memBuffer->getBufferEnd());
}

std::string TextProvider::extract(Location start, Location end) const
{
    std::string str;
    unsigned x = start.getOffset();
    unsigned y = end.getOffset();
    assert(x <= y && "Inconsistent Location range!");
    assert(y < indexOf(memBuffer->getBufferEnd()) && "Locations out of range!");
    str.insert(0, &buffer[x], y - x + 1);
    return str;
}

std::string TextProvider::extract(const TextIterator &s,
                                  const TextIterator &e) const
{
    std::string str;
    unsigned length = e.cursor - s.cursor;
    str.insert(0, s.cursor, length);
    return str;
}

std::string TextProvider::extract(const SourceLocation &sloc) const
{
    assert(sloc.getTextProvider() == this &&
           "SourceLocation not associated with this TextProvider!");

    std::string str;
    unsigned line  = sloc.getLine();
    unsigned start = lines[line - 1];
    unsigned end   = lines[line];
    str.insert(0, &buffer[start], end - start);
    return str;
}

unsigned TextProvider::extract(const TextIterator &s,
                               const TextIterator &e,
                               char *buff, size_t size) const
{
    unsigned length = e.cursor - s.cursor;

    if (buff == 0) return length;

    if (length >= size) {
        ::memcpy(buff, s.cursor, size);
        return size;
    }

    ::memcpy(buff, s.cursor, length);
    buff[length] = 0;
    return length;
}

void TextProvider::initializeLinevec()
{
    lines.push_back(0);
    maxLineIndex = 0;
}

unsigned TextProvider::getLine(Location loc) const
{
    assert(loc < indexOf(memBuffer->getBufferEnd()));

    // Check that loc is within the known range of the line vector. If not,
    // extend the vector with all lines upto this location.
    if (loc > maxLineIndex) {
        unsigned line;
        const char* cursor = &buffer[lines.back()];
        while (cursor != &buffer[loc]) {
            switch (*cursor++) {
            case '\r':
                if (*cursor == '\n')
                    cursor++;
            case '\n':
            case '\f':
                lines.push_back(indexOf(cursor));
            }
        }
        // By returning the length of the vector, we provide a 1-based line
        // number.
        line = lines.size();

        // Continue scanning until the end of the current line is found.
        while (cursor != memBuffer->getBufferEnd()) {
            switch (*cursor++) {
            case '\r':
                if (*cursor == '\n')
                    cursor++;
            case '\n':
            case '\f':
                lines.push_back(indexOf(cursor));
                maxLineIndex = indexOf(cursor);
                return line;
            }
        }
        // We have hit the end of the buffer.
        lines.push_back(indexOf(cursor));
        maxLineIndex = indexOf(cursor);
        return line;
    }

    // Otherwise, perform a binary search over the existing line vector.
    int max = lines.size();
    int start = 0;
    int end = max - 1;
    while (start <= end) {
        int mid = (start + end) >> 1;
        Location candidate = lines[mid];
        if (candidate <= loc) {
            if (mid + 1 < max) {
                if (lines[mid + 1] <= loc) {
                    start = ++mid;
                    continue;
                }
            }
            return ++mid;
        }
        end = --mid;
    }
    assert(false && "Bad offset into chunk map.");
    return 0;
}

unsigned TextProvider::getColumn(Location loc) const
{
    unsigned start = lines[getLine(loc) - 1];
    return loc - start;
}

std::pair<unsigned, unsigned> TextProvider::getLineOf(Location loc) const
{
    unsigned line = getLine(loc) - 1;
    unsigned start = lines[line];
    unsigned end = lines[line + 1];

    return std::pair<unsigned, unsigned>(start, end);
}

