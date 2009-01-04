//===-- parser/Parser.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_PARSER_HDR_GUARD
#define COMMA_PARSER_PARSER_HDR_GUARD

#include "comma/parser/Lexer.h"
#include "comma/parser/Bridge.h"
#include "comma/basic/IdentifierPool.h"
#include "llvm/ADT/SmallVector.h"
#include <iosfwd>

namespace comma {

class Parser {

public:
    Parser(TextProvider &tp, Bridge &bridge, Diagnostic &diag);

    void parseModel();
    void parseModelParameter();
    void parseModelParameterization();
    void parseModelSupersignatures();

    Node parseModelInstantiation();

    // Parses a top level construct.  Returns false once all tokens have been
    // consumed.
    bool parseTopLevelDeclaration();

private:
    TextProvider &tp;
    Bridge &action;
    Diagnostic &diagnostic;

    Lexer lexer;

    Lexer::Token token[2];

    bool seenError;

    // We may wish to refine this typedef into a few classes which provide
    // different sizes which better accomidate adverage demands.
    typedef llvm::SmallVector<Node, 4> NodeVector;

    unsigned currentLine();
    unsigned currentColumn();
    Location currentLocation();

    IdentifierInfo *getIdentifierInfo(const Lexer::Token &tkn);

    Lexer::Token &nextToken();
    Lexer::Token &currentToken();
    Lexer::Token &peekToken();
    void ignoreToken();

    Lexer::Code currentTokenCode();
    Lexer::Code peekTokenCode();

    bool currentTokenIs(Lexer::Code code);
    bool nextTokenIs(Lexer::Code code);
    bool expectToken(Lexer::Code code);
    bool reduceToken(Lexer::Code code);
    bool requireToken(Lexer::Code code);

    bool seekToken(Lexer::Code code);
    bool seekAndConsumeToken(Lexer::Code code);

    bool seekTokens(Lexer::Code code0,
                    Lexer::Code code1 = Lexer::UNUSED_ID,
                    Lexer::Code code2 = Lexer::UNUSED_ID,
                    Lexer::Code code3 = Lexer::UNUSED_ID,
                    Lexer::Code code4 = Lexer::UNUSED_ID);

    bool seekAndConsumeTokens(Lexer::Code code0,
                              Lexer::Code code1 = Lexer::UNUSED_ID,
                              Lexer::Code code2 = Lexer::UNUSED_ID,
                              Lexer::Code code3 = Lexer::UNUSED_ID,
                              Lexer::Code code4 = Lexer::UNUSED_ID);

    bool seekEndLabel(const char *label);

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        SourceLocation sloc = tp.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(SourceLocation sloc, diag::Kind kind) {
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(diag::Kind kind) {
        SourceLocation sloc = tp.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }

    IdentifierInfo *parseIdentifierInfo();

    // Parses an end keyword.  If expected_tag is non-NULL, ensures an end tag
    // matches.  Consumes the terminating semicolon.  Returns true if the parse
    // was sucessful and false otherwise.
    bool parseEndTag(IdentifierInfo *expectedTag = 0);
};

} // End comma namespace

#endif
