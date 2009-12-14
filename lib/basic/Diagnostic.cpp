//===-- basic/Diagnostic.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"

#include <cassert>

using namespace comma;

DiagnosticStream::DiagnosticStream(llvm::raw_ostream &stream)
    : stream(stream), position(0), message(buffer) { }

DiagnosticStream::~DiagnosticStream()
{
    // If the stream was initialized, ensure that it was driven to completion.
    assert(position == 0 && "Diagnostic not driven to completion!");
}

void DiagnosticStream::emitSourceLocation(const SourceLocation &sloc)
{
    std::string identity = sloc.getTextProvider()->getIdentity();
    if (!identity.empty())
        message << identity << ":";
    message << sloc.getLine() << ":" << sloc.getColumn();
}

DiagnosticStream &DiagnosticStream::initialize(const SourceLocation &sloc,
                                               const char *format,
                                               diag::Type type)
{
    assert(position == 0 && "Diagnostic reinitialized before completion!");

    sourceLoc = sloc;
    buffer.clear();
    this->format = format;

    emitSourceLocation(sloc);
    message << ": ";
    emitDiagnosticType(type);
    message << ": ";

    emitFormatComponent();
    return *this;
}

void DiagnosticStream::emitDiagnosticType(diag::Type type)
{
    switch (type) {
    case diag::ERROR:
        message << "error";
        break;
    case diag::WARNING:
        message << "warning";
        break;
    case diag::NOTE:
        message << "note";
        break;
    }
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

    // If we get here, the format string is exhausted.  Print the message and
    // extract the associcated line of source text.  To ensure that diagnostics
    // print properly, strip the source line of any trailing newlines.
    std::string sourceLine = sourceLoc.getTextProvider()->extract(sourceLoc);
    unsigned    column     = sourceLoc.getColumn();
    size_t      endLoc     = sourceLine.find('\n');
    if (endLoc != std::string::npos)
        sourceLine.erase(endLoc);

    stream << message.str() << '\n';
    stream << "  " << sourceLine << '\n';
    stream << "  " << std::string(column, '.') << "^\n";
    position = 0;
    buffer.clear();
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

DiagnosticStream &DiagnosticStream::operator<<(PM::ParameterMode mode)
{
    switch (mode) {
    default:
        assert(false && "Bad parameter mode!");
        break;

    case PM::MODE_DEFAULT:
    case PM::MODE_IN:
        message << "in";
        break;

    case PM::MODE_OUT:
        message << "out";
        break;

    case PM::MODE_IN_OUT:
        message << "in out";
        break;
    }
    emitFormatComponent();
    return *this;
}

//===----------------------------------------------------------------------===//
// Diagnostic

DiagnosticStream &Diagnostic::report(const SourceLocation &loc, diag::Kind kind)
{
    diag::Type type = getType(kind);

    switch (type) {
    case diag::ERROR:
        errorCount++;
        break;
    case diag::WARNING:
        warningCount++;
        break;
    case diag::NOTE:
        noteCount++;
        break;
    }

    return diagstream.initialize(loc, getFormat(kind), type);
}

diag::Type Diagnostic::getType(diag::Kind kind)
{
    return diagnostics[kind].type;
}

const char *Diagnostic::getFormat(diag::Kind kind)
{
    return diagnostics[kind].format;
}

const Diagnostic::DiagInfo
Diagnostic::diagnostics[diag::LAST_UNUSED_DIAGNOSTIC_KIND] = {

#define DIAGNOSTIC(ENUM, TYPE, FORMAT) { FORMAT, diag::TYPE },
#include "comma/basic/Diagnostic.def"
#undef DIAGNOSTIC

};
