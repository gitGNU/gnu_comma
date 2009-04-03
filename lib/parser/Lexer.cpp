//===-- parser/Lexer.cpp -------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Lexer.h"
#include <iostream>
#include <cstring>

using namespace comma;

Lexer::Lexer(TextProvider &txtProvider, Diagnostic &diag)
    : txtProvider(txtProvider),
      diagnostic(diag),
      currentIter(txtProvider.begin()),
      errorDetected(false),
      scanningAborted(false),
      index(0)
{ }

std::string Lexer::Token::getString() const
{
    return Lexer::tokenString(*this);
}

const char *Lexer::tokenString(const Code code)
{
    const char *result;

    switch (code) {
    default:
        result = 0;
        break;

#define RESERVED(NAME, STRING) case TKN_ ## NAME: result = STRING; break;
#define GLYPH(NAME, STRING)    case TKN_ ## NAME: result = STRING; break;
#include "comma/parser/Tokens.def"
#undef RESERVED
#undef GLYPH
    }

    return result;
}

std::string Lexer::tokenString(const Token &token)
{
    Code code = token.getCode();

    switch (code) {
    default:
        return std::string(tokenString(code));
        break;

    case TKN_IDENTIFIER:
    case TKN_INTEGER:
    case TKN_STRING:
        return std::string(token.getRep(), token.getLength());
    }
}

bool Lexer::isDecimalDigit(unsigned c)
{
    return ('0' <= c && c <= '9');
}

bool Lexer::isInitialIdentifierChar(unsigned c)
{
    if (('a' <= c && c <= 'z') ||
        ('A' <= c && c <= 'Z') ||
        (c == '%') || (c == '_'))
        return true;

    return false;
}

bool Lexer::isInnerIdentifierChar(unsigned c)
{
    return isInitialIdentifierChar(c) || isDecimalDigit(c) || c == '?';
}

bool Lexer::isWhitespace(unsigned c)
{
    return (c == ' ') || (c == '\t') || (c == '\n');
}

Location Lexer::currentLocation() const
{
    return txtProvider.getLocation(currentIter);
}

// Something of a fundamental function, since all characters are gathered from
// the underlying stream via this routine.
unsigned Lexer::readStream()
{
    unsigned c = *currentIter;
    ++currentIter;

    // Ensure that carriage returns and DOS style newline sequences are
    // canonicalized into single newline character codes.
    switch (c) {

    case '\r':
        if (*currentIter == '\n')
            ++currentIter;
    case '\n':
        return '\n';
    }

    return c;
}

unsigned Lexer::peekStream()
{
    unsigned c = *currentIter;

    if (c == '\r')
        return '\n';

    return c;
}

void Lexer::ungetStream()
{
    --currentIter;
}

void Lexer::ignoreStream()
{
    readStream();
}

bool Lexer::eatComment()
{
    unsigned c = peekStream();

    if (c == '-') {
        ignoreStream();
        if (peekStream() == '-') {
            // Loop until either a newline or the input stream is
            // exhausted.
            for (;;) {
                c = readStream();
                if (c == '\n' || c == 0)
                    return true;
            }
        }
        else {
            ungetStream();
            return false;
        }
    }
    return false;
}

bool Lexer::eatWhitespace()
{
    unsigned c = peekStream();

    if (isWhitespace(c)) {
        do {
            ignoreStream();
        } while (isWhitespace(c = peekStream()));
        return true;
    }
    return false;
}

void Lexer::emitToken(Code code,
                      const TextIterator &start, const TextIterator &end)
{
    Location    loc    = txtProvider.getLocation(start);
    const char *string = &start;
    unsigned    length = &end - &start;
    *targetToken = Token(code, loc, string, length);
}

void Lexer::emitToken(Code code, Location loc)
{
    *targetToken = Token(code, loc, 0, 0);
}

void Lexer::emitStringToken(const TextIterator &start, const TextIterator &end)
{
    emitToken(TKN_STRING, start, end);
}

void Lexer::emitIntegerToken(const TextIterator &start, const TextIterator &end)
{
    emitToken(TKN_INTEGER, start, end);
}

void Lexer::emitIdentifierToken(const TextIterator &start, const TextIterator &end)
{
    emitToken(TKN_IDENTIFIER, start, end);
}

Lexer::Code Lexer::getTokenCode(TextIterator &start, TextIterator &end) const
{
    Code code = UNUSED_ID;
    const char *str = &start;
    unsigned length = &end - &start;

    switch (length) {
    case 1:
        if (strncmp(str, "%", length) == 0)
            code = TKN_PERCENT;
        break;

    case 2:
        if (strncmp(str, "is", length) == 0)
            code = TKN_IS;
        else if (strncmp(str, "if", length) == 0)
            code = TKN_IF;
        else if (strncmp(str, "in", length) == 0)
            code = TKN_IN;
        break;

    case 3:
        if (strncmp(str, "end", length) == 0)
            code = TKN_END;
        else if (strncmp(str, "out", length) == 0)
            code = TKN_OUT;
        else if (strncmp(str, "add", length) == 0)
            code = TKN_ADD;
        else if (strncmp(str, "inj", length) == 0)
            code = TKN_INJ;
        else if (strncmp(str, "prj", length) == 0)
            code = TKN_PRJ;
        break;

    case 4:
        if (strncmp(str, "else", length) == 0)
            code = TKN_ELSE;
        else if (strncmp(str, "then", length) == 0)
            code = TKN_THEN;
        else if (strncmp(str, "with", length) == 0)
            code = TKN_WITH;
        else if (strncmp(str, "type", length) == 0)
            code = TKN_TYPE;
        break;

    case 5:
        if (strncmp(str, "begin", length) == 0)
            code = TKN_BEGIN;
        else if (strncmp(str, "elsif", length) == 0)
            code = TKN_ELSIF;
        else if (strncmp(str, "while", length) == 0)
            code = TKN_WHILE;
        break;

    case 6:
        if (strncmp(str, "domain", length) == 0)
            code = TKN_DOMAIN;
        else if (strncmp(str, "return", length) == 0)
            code = TKN_RETURN;
        else if (strncmp(str, "import", length) == 0)
            code = TKN_IMPORT;
        else if (strncmp(str, "repeat", length) == 0)
            code = TKN_REPEAT;
        break;

    case 7:
        if (strncmp(str, "carrier", length) == 0)
            code = TKN_CARRIER;
        break;

    case 8:
        if (strncmp(str, "function", length) == 0)
            code = TKN_FUNCTION;
        break;

    case 9:
        if (strncmp(str, "procedure", length) == 0)
            code = TKN_PROCEDURE;
        else if (strncmp(str, "signature", length) == 0)
            code = TKN_SIGNATURE;
        break;
    }
    return code;
}

bool Lexer::scanWord()
{
    TextIterator start = currentIter;
    unsigned c = peekStream();

    if (isInitialIdentifierChar(c)) {
        Code code;

        do {
            ignoreStream();
        } while (isInnerIdentifierChar(c = peekStream()));

        code = getTokenCode(start, currentIter);

        if (code == UNUSED_ID)
            emitIdentifierToken(start, currentIter);
        else
            emitToken(code, txtProvider.getLocation(start));
        return true;
    }
    return false;
}

bool Lexer::scanGlyph()
{
    Location loc = currentLocation();
    unsigned c = readStream();
    Code code  = UNUSED_ID;

    switch (c) {
    case '(':
        code = TKN_LPAREN;
        break;

    case ')':
        code = TKN_RPAREN;
        break;

    case ';':
        code = TKN_SEMI;
        break;

    case '.':
        code = TKN_DOT;
        break;

    case ':':
        switch (peekStream()) {
        case '=':
            ignoreStream();
            code = TKN_ASSIGN;
            break;

        case ':':
            ignoreStream();
            code = TKN_DCOLON;
            break;

        default:
            code = TKN_COLON;
        }
        break;

    case ',':
        code = TKN_COMMA;
        break;

    case '=':
        switch (peekStream()) {
        default:
            code = TKN_EQUAL;
            break;

        case '>':
            ignoreStream();
            code = TKN_RDARROW;
            break;
        }
        break;

    case '[':
        code = TKN_LBRACK;
        break;

    case ']':
        code = TKN_RBRACK;
        break;

    case '{':
        code = TKN_LBRACE;
        break;

    case '}':
        code = TKN_RBRACE;
        break;

    case '+':
        code = TKN_PLUS;
        break;

    case '-':
        code = TKN_MINUS;
        break;

    case '*':
        code = TKN_STAR;
        break;

    case '~':
        switch (peekStream()) {
        case '=':
            ignoreStream();
            code = TKN_NEQUAL;
            break;

        default:
            code = TKN_TILDE;
        }
        break;
    }

    if (code == UNUSED_ID) {
        ungetStream();
        return false;
    }

    emitToken(code, loc);
    return true;
}

bool Lexer::scanEscape()
{
    Location loc = currentLocation();
    unsigned c;

    switch (c = readStream()) {
    case '\\': break;
    case '"' : break;
    case '\'': break;
    case 't' : break;
    case 'n' : break;
    case 'r' : break;
    case 'b' : break;

    case 0:
        // Premature end of stream.  We let this condition be picked up by the
        // caller.
        errorDetected = true;
        ungetStream();
        return false;

    default:
        // Illegal escape sequence.
        report(loc, diag::ILLEGAL_ESCAPE) << (char)c;
        errorDetected = true;
        return false;
    }
    return true;
}

bool Lexer::scanString()
{
    TextIterator start = currentIter;
    Location loc = currentLocation();
    unsigned c;

    if (peekStream() == '"') {
        ignoreStream();

        for (;;) {
            switch (c = readStream()) {
            case '\\':
                // Note that if scanning of the escape fails, we simply do not
                // accumulate the offending sequence and continue scanning.
                scanEscape();
                break;

            case 0:
                // Premature end of stream.  Form the string literal from all
                // tokens accumulated thus far.
                report(loc, diag::UNTERMINATED_STRING);
                errorDetected = true;
                emitStringToken(start, currentIter);
                return true;

            case '\n':
                // Embedded newline.
                report(loc, diag::NEWLINE_IN_STRING_LIT);
                errorDetected = true;
                emitStringToken(start, currentIter);
                return true;

            case '"':
                // End of string literal.
                emitStringToken(start, currentIter);
                return true;
            }
        }
    }
    return false;
}

bool Lexer::scanNumeric()
{
    Location loc = currentLocation();
    TextIterator start = currentIter;
    unsigned c = peekStream();

    if (isDecimalDigit(c)) {
        ignoreStream();

        // Decimal literals cannot have a leading zero (except for the zero
        // literal, of course).  When we spot such a malformed integer, emit a
        // diagnostic and drop the leading zeros.
        if (c == '0' && isDecimalDigit(peekStream())) {
            report(loc, diag::LEADING_ZERO_IN_INTEGER_LIT);
            errorDetected = true;

            while (peekStream() == '0') ignoreStream();

            // Check if we have a string of zeros.  Simply return the zero token
            // in such a case.  Otherwise, continue scanning normally.
            if (!isDecimalDigit(peekStream())) {
                TextIterator end = start;
                emitIntegerToken(start, ++end);
                return true;
            }
            else c = readStream();
        }

        for (;;) {
            c = readStream();

            if (isDecimalDigit(c) || c == '_')
                continue;
            else {
                ungetStream();
                break;
            }
        }
        emitIntegerToken(start, currentIter);
        return true;
    }
    return false;
}

void Lexer::beginExcursion()
{
    positionStack.push_back(index);
}

void Lexer::endExcursion()
{
    index = positionStack.back();
    positionStack.pop_back();
}

void Lexer::forgetExcursion()
{
    unsigned saved_index = positionStack.back();
    positionStack.pop_back();

    if (positionStack.empty()) {
        assert(saved_index == 0 && "index/position mismatch!");
        tokens.clear();
    }
}

void Lexer::peek(Token &tkn, unsigned n)
{
    unsigned numTokens = tokens.size();

    if (index + n < numTokens) {
        tkn = tokens[index + n];
        return;
    }

    unsigned tokensNeeded = index + n - numTokens;
    targetToken = &tkn;
    for (unsigned i = 0; i <= tokensNeeded; ++i) {
        scanToken();
        if (targetToken->getCode() != TKN_EOT)
            tokens.push_back(*targetToken);
    }
}

void Lexer::scan(Token &tkn)
{
    unsigned numTokens = tokens.size();

    // Check if we have a cached token to return.
    if (index < numTokens) {
        tkn = tokens[index++];
        return;
    }

    // Clear the token buffer if it is not empty and we are not in an excursion.
    if (numTokens && positionStack.empty()) {
        tokens.clear();
        index = 0;
    }

    targetToken = &tkn;

    scanToken();

    // Save the token if we are in an excursion and it is not EOT.
    if (!positionStack.empty() && targetToken->getCode() != TKN_EOT) {
        index++;
        tokens.push_back(*targetToken);
    }
}

void Lexer::scanToken()
{
    for (;;) {
        eatWhitespace();
        while (eatComment()) eatWhitespace();

        if (peekStream() == 0 || scanningAborted) {
            emitToken(TKN_EOT, Location());
            return;
        }

        if (scanWord())    return;
        if (scanGlyph())   return;
        if (scanString())  return;
        if (scanNumeric()) return;

        // For invalid character data, simply emit a diagnostic and continue to
        // scan for a token.
        report(diag::INVALID_CHARACTER) << (char)readStream();
        errorDetected = true;
        continue;
    }
}

