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
#include <stack>

namespace comma {

class Parser {

public:
    Parser(TextProvider   &txtProvider,
           IdentifierPool &idPool,
           Bridge         &bridge,
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

    // The following structure is used to collect the components of a parameter
    // of the form "X : T", where X is an IdentifierInfo representing the
    // parameter name, and T is a Node representing the associcate type.  In
    // addition, the locations of the parameter and type are provided.
    struct ParameterInfo {
        IdentifierInfo *formal;
        Node            type;
        Location        formalLocation;
        Location        typeLocation;
    };

    // Parses a formal parameter of the form "X : T", filling in paramInfo with
    // the results of the parse.  The supplied parse method is invoked to
    // consume the rhs of the ":" -- namely, the type expression.  This function
    // returns true if the parse was successful.  If the parse was not
    // successful false is returned and paramInfo is not modified.
    bool parseFormalParameter(ParameterInfo &paramInfo, parseNodeFn parser);

    // Parses a list of formal parameters.  An opening `(' is expected on the
    // stream when this function is called.  The supplied vector is populated
    // with ParameterInfo structures corresponding to each parameter sucessfully
    // parsed.  True is returned if no errors were encountered and false
    // otherwise.  In the case of a parsing error, this function attempts to
    // position itself with the closing paren consumed, and with `return', `is',
    // `;' or EOT as the current token.
    bool parseFormalParameterList(std::vector<ParameterInfo> &params);

    bool parseSubroutineParameter(Descriptor &desc);
    void parseSubroutineParameters(Descriptor &desc);
    Node parseFunctionDeclaration(bool allowBody = true);
    Node parseFunctionProto();

    Node parseProcedureDeclaration(bool allowBody = true);
    Node parseProcedureProto();

    Node parseSubroutineParmeterList();
    void parseSubroutineBody(IdentifierInfo *endTag);

    Node parseValueDeclaration(bool allowInitializer = true);

    void parseAddComponents();

    Node parseDeclaration();
    Node parseStatement();

    // Parses a top level construct.  Returns false once all tokens have been
    // consumed.
    bool parseTopLevelDeclaration();

private:
    TextProvider   &txtProvider;
    IdentifierPool &idPool;
    Bridge         &action;
    Diagnostic     &diagnostic;

    Lexer lexer;

    Lexer::Token token[2];

    bool seenError;

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
            action.deleteNode(*iter);
    }

};

} // End comma namespace

#endif
