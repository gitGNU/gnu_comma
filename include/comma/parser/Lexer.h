//===-- parser/Lexer.h ---------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_LEXER_HDR_GUARD
#define COMMA_PARSER_LEXER_HDR_GUARD

#include "comma/basic/Diagnostic.h"
#include "comma/basic/TextProvider.h"
#include <iosfwd>
#include <string>

namespace comma {

class Lexer {

public:
    Lexer(TextProvider &txtProvider, Diagnostic &diag);

    // Aside from UNUSED_ID, the only codes which should be used by clients of
    // this class are those prefixed by TKN.  All other codes are considered
    // internal and are subject to change.  Questions about a codes kind (for
    // example, determining if a code represents a reserved word) should be
    // answered thru the supplied predicates and not be based on this
    // enumerations ordering.
    enum Code {
        UNUSED_ID,

#define RESERVED(NAME, STRING) TKN_ ## NAME,
#define GLYPH(NAME, STRING)    TKN_ ## NAME,
#define TOKEN(NAME)            TKN_ ## NAME,
#include "comma/parser/Tokens.def"
#undef RESERVED
#undef GLYPH
#undef TOKEN

        NUMTOKEN_CODES
    };

    // The Token class represents the result of lexing process.  Tokens are
    // identified by code.  They provide access to their underlying string
    // representation, and have position information in the form of a single
    // Location entry (which must be interpreted with respect to a particular
    // TextProvider).
    class Token {

    public:
        Token() : code(Lexer::UNUSED_ID) { }

        Lexer::Code getCode() const { return code; }

        Location getLocation() const { return location; }

        const char *getRep() const { return string; }

        unsigned getLength() const { return length; }

        // This method provides a string representation of the token.
        std::string getString() const;

    private:
        Lexer::Code code   : 8;
        unsigned    length : 24;
        Location    location;
        const char *string;

        // Declare Lexer as a friend to give access to the following
        // constructor.
        friend class Lexer;

        Token(Lexer::Code code,
              Location    location,
              const char *string,
              unsigned length)
            : code(code),
              length(length),
              location(location),
              string(string) { }
    };

    // Scans a single token from the input stream.  When the stream is
    // exhausted, all further calls to this method will set the supplied tokens
    // code to TKN_EOT.
    void scan(Lexer::Token &tkn);

    void peek(Lexer::Token &tkn, unsigned n);

    // Saves the current "position" of the lexer.  Further calls to Lexer::scan
    // will remember the resulting tokens.  The token stream can be restored to
    // the state before saveExcursion was called with a call to
    // Lexer::endExcursion.  Alternatively, the excursion can be forgotten with
    // a call to Lexer::forgetExcursion.
    void beginExcursion();

    void endExcursion();

    void forgetExcursion();

    // Returns true if the lexer has not seen an error while scanning its input.
    bool lexSuccessful() const { return errorCount == 0; }

    // Returns the number of errors seen by the lexer.
    unsigned getErrorCount() const { return errorCount; }

    /// Calling this function prematurely aborts scanning.  All further calls to
    /// Lexer::scan will result in a TKN_EOT token thereby ending the lexical
    /// analysis.
    void abortScanning() { scanningAborted = true; }

    // Returns true if the given token is a glyph and it can name a function
    // (e.g. '+', '*', etc).
    static bool isFunctionGlyph(const Lexer::Token &tkn) {
        switch (tkn.getCode()) {
        case TKN_EQUAL:
        case TKN_LESS:
        case TKN_LEQ:
        case TKN_GREAT:
        case TKN_GEQ:
        case TKN_MINUS:
        case TKN_STAR:
        case TKN_PLUS:
        case TKN_POW:
            return true;
        default:
            return false;
        }
    }


    // Returns a static string representation of the given token code, or NULL
    // if no such representation is available.
    static const char *tokenString(Code code);

    // Returns the string representation of the given token.
    static std::string tokenString(const Token &tkn);

private:
    void scanToken();

    bool eatWhitespace();

    bool eatComment();

    bool scanWord();

    bool scanGlyph();

    bool scanCharacter();

    bool scanString();

    bool scanNumeric();

    bool scanEscape();

    static bool isAlphabetic(unsigned c);

    static bool isInitialIdentifierChar(unsigned c);

    static bool isInnerIdentifierChar(unsigned c);

    static bool isWhitespace(unsigned c);

    static bool isDecimalDigit(unsigned c);

    Location currentLocation() const;

    // We do not read from the supplied stream directly.  Rather, we call the
    // following simple wrappers which ensure that our line and column
    // information is consistently updated.  Carriage returns and DOS style
    // newlines are canonicalized to single newline characters.  When the stream
    // is exhausted, 0 is returned to signal EOF.
    unsigned readStream();
    unsigned peekStream();
    void ungetStream();
    void ignoreStream();

    // Returns the token code assoicated with the chatacter string delimited by
    // start and end.  If the string exactly matches a reserved word (this
    // function will not recignize glyph tokens) the words corresponding code is
    // returned, else UNUSED_ID if there is no match.
    Code getTokenCode(TextIterator &start, TextIterator &end) const;

    void emitToken(Code code,
                   const TextIterator &start, const TextIterator &end);

    // Used to emit reserved identifiers and glyphs which do not require a
    // string representation.
    void emitToken(Code code, Location loc);

    void emitStringToken(const TextIterator &start, const TextIterator &end);

    void emitIntegerToken(const TextIterator &start, const TextIterator &end);

    void emitIdentifierToken(const TextIterator &start,
                             const TextIterator &end);

    void emitCharacterToken(const TextIterator &start, const TextIterator &end);

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        ++errorCount;
        SourceLocation sloc = txtProvider.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }

    // If c1 and c2 are both '_' characters, report a consecutive underscore
    // error and drive the stream to consume all remaining '_' characters.
    void diagnoseConsecutiveUnderscores(unsigned c1, unsigned c2);

    DiagnosticStream &report(SourceLocation sloc, diag::Kind kind)  {
        ++errorCount;
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(diag::Kind kind) {
        ++errorCount;
        SourceLocation sloc = txtProvider.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }

    // This is the stream we read from.
    TextProvider &txtProvider;

    // Output stream to send messages.
    Diagnostic &diagnostic;

    // An iterator into our stream.
    TextIterator currentIter;

    // Numer of errors detected.
    unsigned errorCount;

    // True is lexing has been cancelled with a call to abortScanning().
    bool scanningAborted;

    // The token parameter supplied to scan() is maintained here.  This is
    // the destination of the lexing methods.
    Token *targetToken;

    // A vector of tokens is used to implement lookahead.
    std::vector<Token> tokens;

    // A stack of positions into the token vector, used to implement
    // savePosition and restorePosition.
    std::vector<unsigned> positionStack;

    // Index into our token vector.  This index is non-zero only when an
    // excursion has ended with a call to endExcursion.
    unsigned index;
};

} // End comma namespace

#endif
