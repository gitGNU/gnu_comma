//===-- basic/Diagnostic.h ------------------------------------ -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_BASIC_DIAGNOSTIC_HDR_GUARD
#define COMMA_BASIC_DIAGNOSTIC_HDR_GUARD

#include "comma/basic/IdentifierInfo.h"
#include "comma/basic/Location.h"
#include "comma/basic/ParameterModes.h"

#include "llvm/Support/raw_ostream.h"

namespace comma {

namespace diag {

enum Kind {
#define DIAGNOSTIC(ENUM, KIND, FORMAT) ENUM,
#include "comma/basic/Diagnostic.def"
#undef DIAGNOSTIC

    LAST_UNUSED_DIAGNOSTIC_KIND
};

/// Every diagnostic is associated with one of the following types.
enum Type {
    ERROR,             ///< Hard error conditions.
    WARNING,           ///< Warning conditions.
    NOTE               ///< Informative notes.
};

} // End diag namespace.

/// \class
///
/// \brief Simple virtual interface which allows subclasses to print themselves
/// to a DiagnosticStream.
class DiagnosticComponent {

public:
    virtual ~DiagnosticComponent() { }

    /// Calls to this method should produce output to the given stream.
    virtual void print(llvm::raw_ostream &stream) const = 0;
};

class DiagnosticStream {

public:
    DiagnosticStream(llvm::raw_ostream &stream);

    ~DiagnosticStream();

    llvm::raw_ostream &getStream() { return stream; }

    DiagnosticStream &initialize(const SourceLocation &sloc, const char *format,
                                 diag::Type type);

    DiagnosticStream &operator<<(const std::string &string);

    DiagnosticStream &operator<<(const char *string);

    DiagnosticStream &operator<<(int n);

    DiagnosticStream &operator<<(char c);

    DiagnosticStream &operator<<(const SourceLocation &sloc);

    DiagnosticStream &operator<<(const IdentifierInfo *idInfo);

    DiagnosticStream &operator<<(PM::ParameterMode mode);

    DiagnosticStream &operator<<(const DiagnosticComponent &component);

private:
    void emitFormatComponent();

    void emitSourceLocation(const SourceLocation &sloc);

    void emitDiagnosticType(diag::Type type);

    llvm::raw_ostream &stream;
    unsigned position;
    std::string buffer;
    llvm::raw_string_ostream message;
    SourceLocation sourceLoc;
    const char *format;
    diag::Type type;
};

class Diagnostic {

public:
    /// Creates a diagnostic object with the reporting stream defaulting to
    /// std::cerr;
    Diagnostic() :
        diagstream(llvm::errs()),
        errorCount(0), warningCount(0), noteCount(0) { }

    /// Creates a diagnostic object with the given output stream serving as the
    /// default stream to which messages are delivered.
    Diagnostic(llvm::raw_ostream &stream) : diagstream(stream) { }

    /// Returns a DiagnosticStream which is ready to accept the arguments
    /// required by the diagnostic \p kind.
    DiagnosticStream &report(const SourceLocation &loc, diag::Kind kind);

    /// Returns a DiagnosticStream which is ready to accept the arguments
    /// required by the diagnostic \p kind, but will not propagate location
    /// information.
    DiagnosticStream &report(diag::Kind kind);

    /// Returns the format control string for the given diagnostic kind.
    const char *getFormat(diag::Kind kind);

    /// Returns the type associated with the given diagnostic kind.
    diag::Type getType(diag::Kind kind);

    /// Returns true if report() has been called.
    bool reportsGenerated() { return numReports() != 0; }

    /// Returns the number of reports handled by this Diagnostic so far.
    unsigned numReports() const {
        return errorCount + warningCount + noteCount;
    }

    /// Returns the number of error diagnostics posted.
    unsigned numErrors() const { return errorCount; }

    /// Returns the number of warning diagnostics posted.
    unsigned numWarnings() const { return warningCount; }

    /// Returns the number of note diagnostics posted.
    unsigned numNotes() const { return noteCount; }

    /// Returns the stream this Diagnostic object posts to.
    llvm::raw_ostream &getStream() { return diagstream.getStream(); }

private:
    DiagnosticStream diagstream;

    unsigned errorCount;
    unsigned warningCount;
    unsigned noteCount;

    static void initializeMessages();

    struct DiagInfo {
        const char *format;
        diag::Type type;
    };

    static const DiagInfo diagnostics[diag::LAST_UNUSED_DIAGNOSTIC_KIND];
};

} // End comma namespace.

#endif
