//===-- basic/Diagnostic.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/Diagnostic.h"
#include <cassert>

using namespace comma;

DiagnosticStream::DiagnosticStream(std::ostream &stream)
    : stream(stream), position(0) { }

void DiagnosticStream::emitSourceLocation(const SourceLocation &sloc)
{
    if (!sloc.getIdentity().empty())
        message << sloc.getIdentity() << ":";
    message << sloc.getLine() << ":" << sloc.getColumn();
}

DiagnosticStream &DiagnosticStream::initialize(const SourceLocation &sloc,
                                               const char *format)
{
    assert(position == 0 && "Diagnostic reinitialized before completion!");

    message.str("");
    this->format = format;
    emitSourceLocation(sloc);
    message << ": ";
    emitFormatComponent();
    return *this;
}

void DiagnosticStream::emitFormatComponent()
{
    for (char c = format[position]; c; c = format[++position]) {
        if (c == '%') {
            c = format[++position];
            assert(c != 0 && "Malformed diagnostic format string!");
            if (c == '%') {
                message << c;
                continue;
            }
            ++position;
            return;
        }
        message << c;
    }
    // If we get here, the format string is exhausted.  Publish the accumulated
    // format control and reset our position.
    stream << message.str() << std::endl;
    position = 0;
    message.str("");
}

DiagnosticStream &DiagnosticStream::operator<<(const std::string &string)
{
    message << string;
    emitFormatComponent();
    return *this;
}

DiagnosticStream &DiagnosticStream::operator<<(const char *string)
{
    message << string;
    emitFormatComponent();
    return *this;
}

DiagnosticStream &DiagnosticStream::operator<<(int n)
{
    message << n;
    emitFormatComponent();
    return *this;
}

DiagnosticStream &DiagnosticStream::operator<<(char c)
{
    message << c;
    emitFormatComponent();
    return *this;
}

DiagnosticStream &DiagnosticStream::operator<<(const SourceLocation &sloc)
{
    emitSourceLocation(sloc);
    emitFormatComponent();
    return *this;
}

DiagnosticStream &DiagnosticStream::operator<<(const IdentifierInfo *idInfo)
{
    message << idInfo->getString();
    emitFormatComponent();
    return *this;
}

DiagnosticStream &Diagnostic::report(const SourceLocation &loc, diag::Kind kind)
{
    return diagstream.initialize(loc, messages[kind]);
}

const char *Diagnostic::messages[diag::LAST_UNUSED_DIAGNOSTIC_KIND] = {
#define DIAGNOSTIC(ENUM, KIND, FORMAT) FORMAT,
#include "comma/basic/Diagnostic.def"
#undef DIAGNOSTIC
};
