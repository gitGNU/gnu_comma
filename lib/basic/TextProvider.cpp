//===-- basic/TextProvider.cpp -------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/TextProvider.h"
#include <ostream>
#include <cstring>
#include <cassert>

using namespace comma;

TextProvider::TextProvider(const llvm::sys::Path &path)
{
    // FIXME:  We should be checking for failure here.
    memBuffer = llvm::MemoryBuffer::getFile(path.c_str());
    buffer = memBuffer->getBufferStart();
    identity = path.getLast();
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
    unsigned line = getLine(ti);
    unsigned column = getColumn(ti);
    return SourceLocation(line, column, identity);
}

SourceLocation TextProvider::getSourceLocation(const Location loc) const
{
    unsigned line = getLine(loc);
    unsigned column = getColumn(loc);
    return SourceLocation(line, column, identity);
}

TextIterator TextProvider::begin() const
{
    return TextIterator(buffer);
}


TextIterator TextProvider::end() const
{
    return TextIterator(memBuffer->getBufferEnd());
}

void TextProvider::extract(const TextIterator &s,
                           const TextIterator &e, std::string &str) const
{
    unsigned length = e.cursor - s.cursor;
    str.insert(0, s.cursor, length);
}

std::string TextProvider::extract(unsigned s, unsigned e) const
{
    std::string str;
    str.insert(0, &buffer[s], e - s);
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

std::ostream& comma::printLocation(std::ostream &s,
                                   Location loc, TextProvider &tp)
{
    unsigned line = tp.getLine(loc);
    unsigned column = tp.getColumn(loc);

    s << line << ':' << column << ':';
    return s;
}

std::pair<unsigned, unsigned> TextProvider::getLineOf(Location loc) const
{
    unsigned line = getLine(loc) - 1;
    unsigned start = lines[line];
    unsigned end = lines[line + 1];

    return std::pair<unsigned, unsigned>(start, end);
}

