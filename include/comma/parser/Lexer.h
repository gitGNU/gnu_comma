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

        FIRST_KEYWORD,
        FIRST_STATIC_CODE = FIRST_KEYWORD,

        TKN_ADD           = FIRST_KEYWORD,
        TKN_BEGIN,
        TKN_BODY,
        TKN_DOMAIN,
        TKN_ELSE,
        TKN_ELSIF,
        TKN_END,
        TKN_FOR,
        TKN_FUNCTION,
        TKN_IF,
        TKN_IS,
        TKN_MODULE,
        TKN_REPEAT,
        TKN_RETURN,
        TKN_SIGNATURE,
        TKN_THEN,
        TKN_WHILE,
        TKN_WITH,

        LAST_KEYWORD      = TKN_WITH,
        FIRST_GLYPH,

        TKN_COMMA         = FIRST_GLYPH,
        TKN_COLON,
        TKN_DCOLON,
        TKN_DOT,
        TKN_EQUAL,
        TKN_MINUS,
        TKN_NEQUAL,
        TKN_PLUS,
        TKN_RDARROW,
        TKN_SEMI,
        TKN_STAR,
        TKN_TILDE,
        TKN_ASSIGN,
        TKN_LBRACE,
        TKN_RBRACE,
        TKN_LBRACK,
        TKN_RBRACK,
        TKN_LPAREN,
        TKN_RPAREN,

        LAST_GLYPH        = TKN_RPAREN,

        TKN_PERCENT,
        TKN_EOT,

        LAST_STATIC_CODE  = TKN_EOT,

        TKN_IDENTIFIER,
        TKN_STRING,
        TKN_INTEGER,
        TKN_FLOAT,
        TKN_CHARACTER
    };

    // The Token class represents the result of lexing process.  Tokens are
    // identified by code.  They provide access to their underlying string
    // representation, and have position information in the form of line and
    // column (1-based) coordinates.
    class Token {

    public:
        Token() : code(Lexer::UNUSED_ID) { }

        Lexer::Code getCode() const { return code; }

        Location getLocation() const { return location; }

        const char *getRep() const { return string; }

        unsigned getLength() const { return length; }

        // This method provides a string representation of the token. The only
        // tokens which do not have string representations are TKN_EOT,
        // TKN_ERROR, and UNUSED_ID.  When called on any one of these codes this
        // method returns 0.
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

    // Returns true if the lexer has seen an error while scanning its input.
    bool seenError() const { return errorDetected; }

    static bool isReservedWord(Lexer::Code code) {
        return (FIRST_KEYWORD <= code && code <= LAST_KEYWORD);
    }

    static bool isReservedWord(const Lexer::Token &tkn) {
        return isReservedWord(tkn.getCode());
    }

    static bool isGlyph(Lexer::Code code) {
        return (FIRST_GLYPH <= code && code <= LAST_GLYPH);
    }

    static bool isGlyph(const Lexer::Token &tkn) {
        return isGlyph(tkn.getCode());
    }

    // Returns true if the given token is a glyph and it can name a function
    // (e.g. '+', '*', etc).
    static bool isFunctionGlyph(const Lexer::Token &tkn) {
        switch (tkn.getCode()) {
        case TKN_EQUAL:
        case TKN_MINUS:
        case TKN_NEQUAL:
        case TKN_STAR:
        case TKN_PLUS:
        case TKN_TILDE:
            return true;
        default:
            return false;
        }
    }

    // Returns true if the given code has a known string representation.
    static bool hasString(Lexer::Code code) {
        return (FIRST_STATIC_CODE <= code && code <= LAST_STATIC_CODE);
    }

    static bool hasString(const Lexer::Token &tkn) {
        return hasString(tkn.getCode());
    }

    static const char *tokenString(Code code);

    static const char *tokenString(const Lexer::Token &tkn) {
        return tokenString(tkn.getCode());
    }

private:
    bool eatWhitespace();

    bool eatComment();

    bool scanWord();

    bool scanGlyph();

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

    // Used to emit keywords and glyphs which do not require a string
    // representation.
    void emitToken(Code code, Location loc);

    void emitStringToken(const TextIterator &start, const TextIterator &end);

    void emitIntegerToken(const TextIterator &start, const TextIterator &end);

    void emitIdentifierToken(const TextIterator &start,
                             const TextIterator &end);

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(SourceLocation sloc, diag::Kind kind)  {
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }

    // This is the stream we read from.
    TextProvider &txtProvider;

    // Output stream to send messages.
    Diagnostic &diagnostic;

    // An iterator into our stream.
    TextIterator currentIter;

    // Flag indicating if a lexical error has been detected.
    bool errorDetected;

    // The token parameter supplied to scan() is maintained here.  This is
    // the destination of the lexing methods.
    Token *targetToken;

    // Static storage for all tokens with a predefined string
    // representation.
    static const char *tokenStrings[LAST_STATIC_CODE - FIRST_STATIC_CODE + 1];
};

} // End comma namespace

#endif
