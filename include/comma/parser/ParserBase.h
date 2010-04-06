//===-- parser/ParserBase.h ----------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_PARSERBASE_HDR_GUARD
#define COMMA_PARSER_PARSERBASE_HDR_GUARD

#include "comma/basic/IdentifierPool.h"
#include "comma/parser/Lexer.h"

namespace comma {

/// \class
/// \brief Provides common functionality for various Comma parsers.
class ParserBase {

public:
    virtual ~ParserBase() { }

    /// \brief Returns true if this parser has been driven without seeing an
    /// error and false otherwise.
    bool parseSuccessful() const { return diagnostic.numErrors() == 0; }


    ParserBase(TextProvider &txtProvider, IdentifierPool &idPool,
               Diagnostic &diag);

protected:
    /// Returns the current line.
    unsigned currentLine();

    /// Returns the current column.
    unsigned currentColumn();

    /// Returns the current Location object.
    Location currentLocation();

    /// Advances the token stream and returns the next token.
    Lexer::Token &nextToken();

    /// Returns the current token.
    Lexer::Token &currentToken();

    /// Returns the next token without advancing the stream.
    Lexer::Token  peekToken();

    /// Replaces the currect token with \p tkn.
    void setCurrentToken(Lexer::Token &tkn);

    /// \brief Ignores the current token and returns the location of the token
    /// skipped.
    ///
    /// This method is used to support the idiom of skipping reserved words and
    /// saving the location of the construct.
    Location ignoreToken();

    /// Returns the code of the current token.
    Lexer::Code currentTokenCode();

    /// Returns the code of the next token without advancing the token stream.
    Lexer::Code peekTokenCode();

    /// Returns true if the current token has the given \p code.
    bool currentTokenIs(Lexer::Code code);

    /// Returns true if the next token on the stream has the given \p code.
    bool nextTokenIs(Lexer::Code code);

    /// If the next token on the stream has the given \p code, return true and
    /// advances the token stream.  Otherwise returns false and the stream is
    /// not advanced.
    bool expectToken(Lexer::Code code);

    /// If the current token on the stream has the given \p code, returns true
    /// and advances the token stream.  Otherwise returns false and the stream
    /// is not advanced.
    bool reduceToken(Lexer::Code code);

    /// Like reduceToken, but if the current token does not have the given \p
    /// code, an UNEXPECTED_TOKEN_WANTED error is posted.
    bool requireToken(Lexer::Code code);

    /// Scans each token on the stream, including the current token, searching
    /// for the given \p code.  The first token found is made current and true
    /// is returned.  Otherwise the stream is exhausted with TKN_EOT as current
    /// and false is returned.
    bool seekToken(Lexer::Code code);

    /// Like seekToken, but consumes the matching token, if any.
    bool seekAndConsumeToken(Lexer::Code code);

    /// Like seekToken, but searches for the first match from the given set of
    /// codes.
    bool seekTokens(Lexer::Code code0,
                    Lexer::Code code1 = Lexer::UNUSED_ID,
                    Lexer::Code code2 = Lexer::UNUSED_ID,
                    Lexer::Code code3 = Lexer::UNUSED_ID,
                    Lexer::Code code4 = Lexer::UNUSED_ID,
                    Lexer::Code code5 = Lexer::UNUSED_ID);

    /// Like seekAndConsumeToken, but searches for the first match from the
    /// given set of codes.
    bool seekAndConsumeTokens(Lexer::Code code0,
                              Lexer::Code code1 = Lexer::UNUSED_ID,
                              Lexer::Code code2 = Lexer::UNUSED_ID,
                              Lexer::Code code3 = Lexer::UNUSED_ID,
                              Lexer::Code code4 = Lexer::UNUSED_ID);

    /// Returns a std::string representation of the current token.
    std::string currentTokenString() {
        return currentToken().getString();
    }

    /// Returns the IdentifierInfo corresponding to the given token.
    IdentifierInfo *getIdentifierInfo(const Lexer::Token &tkn);

    /// Begins a posting to the diagnostic stream with the given Location and
    /// diagnostic kind.
    DiagnosticStream &report(Location loc, diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }

    /// Begins a posting to the diagnostic stream with the given SourceLocation
    /// and diagnostic kind.
    DiagnosticStream &report(SourceLocation sloc, diag::Kind kind) {
        return diagnostic.report(sloc, kind);
    }

    /// Begins a posting to the diagnostic stream with the given diagnostic kind
    /// and uses the current location to qualify the report.
    DiagnosticStream &report(diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }

    /// Saves the current token stream, enabling speculative parsing.
    void beginExcursion();

    /// Restores the token stream to the state it was in before the last call to
    /// beginExcursion().
    void endExcursion();

    /// Terminates an excursion without restoring the token stream to its
    /// previous state.
    void forgetExcursion();

    //===------------------------------------------------------------------===//
    // Basic Parser Methods.

    /// Parses a Comma identifier.  Returns the parsed identifier or null on
    /// error.  In the latter case, diagnostics are posted.
    IdentifierInfo *parseIdentifier();

    /// Parses a Comma subroutine identifier.  Returns the parsed identifier or
    /// null on error.  In the latter case, diagnostics are posted.
    IdentifierInfo *parseFunctionIdentifier();

private:
    TextProvider   &txtProvider;
    Diagnostic     &diagnostic;
    IdentifierPool &idPool;
    Lexer           lexer;

    /// Current token being parsed.
    Lexer::Token token;

    /// Vector of tokens corresponding to the token that was current at the
    /// start of an excursion.
    std::vector<Lexer::Token> savedTokens;
};

//===----------------------------------------------------------------------===//
// Inline methods

inline Location ParserBase::currentLocation()
{
    return currentToken().getLocation();
}

inline unsigned ParserBase::currentLine()
{
    return txtProvider.getLine(currentLocation());
}

inline unsigned ParserBase::currentColumn()
{
    return txtProvider.getColumn(currentLocation());
}

inline Lexer::Token &ParserBase::currentToken()
{
    return token;
}

inline Lexer::Token &ParserBase::nextToken()
{
    lexer.scan(token);
    return token;
}

inline Lexer::Token ParserBase::peekToken()
{
    Lexer::Token tkn;
    lexer.peek(tkn, 0);
    return tkn;
}

inline Location ParserBase::ignoreToken()
{
    Location loc = currentLocation();
    nextToken();
    return loc;
}

inline void ParserBase::setCurrentToken(Lexer::Token &tkn)
{
    token = tkn;
}

inline bool ParserBase::currentTokenIs(Lexer::Code code)
{
    return currentToken().getCode() == code;
}

inline bool ParserBase::nextTokenIs(Lexer::Code code)
{
    return peekToken().getCode() == code;
}

inline Lexer::Code ParserBase::currentTokenCode()
{
    return currentToken().getCode();
}

inline Lexer::Code ParserBase::peekTokenCode()
{
    return peekToken().getCode();
}

inline bool ParserBase::expectToken(Lexer::Code code)
{
    if (peekToken().getCode() == code) {
        ignoreToken();
        return true;
    }
    return false;
}

inline bool ParserBase::reduceToken(Lexer::Code code)
{
    if (currentTokenIs(code)) {
        ignoreToken();
        return true;
    }
    return false;
}

} // end comma namespace.

#endif
