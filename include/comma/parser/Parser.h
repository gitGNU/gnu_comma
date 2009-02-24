//===-- parser/Parser.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#ifndef COMMA_PARSER_PARSER_HDR_GUARD
#define COMMA_PARSER_PARSER_HDR_GUARD

#include "comma/basic/IdentifierPool.h"
#include "comma/basic/ParameterModes.h"
#include "comma/parser/Lexer.h"
#include "comma/parser/ParseClient.h"
#include "llvm/ADT/SmallVector.h"
#include <iosfwd>
#include <stack>

namespace comma {

class Parser {

public:
    Parser(TextProvider   &txtProvider,
           IdentifierPool &idPool,
           ParseClient    &client,
           Diagnostic     &diag);

    // Generic pointer to a parse method.
    typedef void (Parser::*parseFn)();

    // Generic pointer to a parse method returning a Node.
    typedef Node (Parser::*parseNodeFn)();

    void parseModelDeclaration(Descriptor &desc);
    void parseModel();

    Node parseModelParameter();
    void parseModelParameterization(Descriptor &desc);

    void parseWithExpression();
    void parseWithSupersignatures();
    void parseWithDeclarations();

    void parseSignatureDecls();

    Node parseModelInstantiation();

    bool parseSubroutineParameter(Descriptor &desc);
    void parseSubroutineParameters(Descriptor &desc);

    Node parseFunctionDeclaration();
    Node parseProcedureDeclaration();
    Node parseFunctionDeclaration(Descriptor &desc);
    Node parseProcedureDeclaration(Descriptor &desc);

    /// This parser is called just after the 'is' token beginning a function or
    /// procedure definition.  The argument \p declarationNode is a valid Node
    /// returned from a call to Parser::parseSubroutineDeclaration.  The
    /// endTagStack must hold the expected end tag name.
    void parseSubroutineBody(Node declarationNode);

    Node parseObjectDeclaration();
    Node parseImportDeclaration();

    void parseCarrier();
    void parseAddComponents();

    Node parseDeclaration();
    Node parseStatement();

    Node parseSubroutineKeywordSelection();
    Node parseProcedureCallStatement();

    Node parseExpr();
    Node parsePrimaryExpr();

    // Parses a top level construct.  Returns false once all tokens have been
    // consumed.
    bool parseTopLevelDeclaration();

private:
    TextProvider   &txtProvider;
    IdentifierPool &idPool;
    ParseClient    &client;
    Diagnostic     &diagnostic;
    Lexer           lexer;

    // A small buffer of two tokens is maintained to provide one token of
    // lookahead.  The current token is at index 0, and the next token is at
    // index 1.  When we consume a token, we set token[0] = token[1], and set
    // token[1] to the next token emmited by the lexer.
    Lexer::Token token[2];

    // Set to true when the parser encounters an error.
    bool seenError;

    // The kind of end tag which is expected.  This enumeration will be
    // expanded.
    enum EndTagKind {
        NAMED_TAG               ///< end tag followed by a name.
    };

    // As the parser encounters constructs which must be matched by an end tag
    // an EndTagEntry is pushed onto the EndTagStack.
    struct EndTagEntry {

        EndTagEntry(EndTagKind kind, Location loc, IdentifierInfo *tag = 0)
            : kind(kind), location(loc), tag(tag) { }

        // The kind of end tag expected.
        EndTagKind kind;

        // Location of the construct introducing this scope.  Used for reporting
        // nesting errors or tag missmatches.
        Location location;

        // The expected tag or NULL if no tag is required.
        IdentifierInfo *tag;
    };

    std::stack<EndTagEntry> endTagStack;

    // We may wish to refine this typedef into a few classes which provide
    // different sizes which better accomidate adverage demands.
    typedef llvm::SmallVector<Node, 4> NodeVector;

    // Generic vector type to accumulate locations.
    typedef llvm::SmallVector<Location, 4> LocationVector;

    // Accumulation of IdentifierInfo nodes.
    typedef llvm::SmallVector<IdentifierInfo*, 4> IdInfoVector;

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

    bool seekCloseParen();

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

    std::string currentTokenString() {
        return currentToken().getString();
    }

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(SourceLocation sloc, diag::Kind kind) {
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(diag::Kind kind) {
        SourceLocation sloc = txtProvider.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }

    IdentifierInfo *parseIdentifierInfo();
    IdentifierInfo *parseFunctionIdentifierInfo();

    ParameterMode parseParameterMode();

    Node parseSubroutineDeclaration(Descriptor &desc);

    bool seekEndTag(IdentifierInfo *tag);

    bool seekAndConsumeEndTag(IdentifierInfo *tag);

    // Parses an end keyword.  If expected_tag is non-NULL, ensures an end tag
    // matches.  Consumes the terminating semicolon.  Returns true if the parse
    // was sucessful and false otherwise.
    bool parseEndTag(IdentifierInfo *expectedTag = 0);

    // Returns true if a matching pair of parens "()" is next on the stream of
    // tokens.
    bool unitExprFollows();

    // Returns true if a keyword selection expression follows:  That is, if
    // the token stream admits an IdentifierInfo followed by a '=>' token.
    bool keywordSelectionFollows();

    // Convenience function for deleting a collection of Node's.
    template <class T> void deleteNodes(T &nodes) {
        typename T::iterator iter;
        typename T::iterator endIter = nodes.end();
        for (iter = nodes.begin(); iter != endIter; ++iter)
            client.deleteNode(*iter);
    }

};

} // End comma namespace

#endif
