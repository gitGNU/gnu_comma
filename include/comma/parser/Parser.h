//===-- parser/Parser.h --------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
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

namespace llvm {

class APInt;

} // end llvm namespace.

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

    void parseModel();

    void parseGenericFormalParams();
    void parseGenericFormalDomain();

    void parseSignatureProfile();
    void parseSupersignatureProfile();
    void parseWithProfile();

    Node parseModelApplication(Node qualNode);
    Node parseModelInstantiation();

    bool parseSubroutineParameter(Descriptor &desc);
    void parseSubroutineParameters(Descriptor &desc);

    Node parseFunctionDeclaration();
    Node parseProcedureDeclaration();
    Node parseFunctionDeclaration(Descriptor &desc);
    Node parseProcedureDeclaration(Descriptor &desc);
    void parseFunctionDeclOrDefinition();
    void parseProcedureDeclOrDefinition();
    void parseOverrideTarget(Node declarationNode);

    /// This parser is called just after the 'is' token beginning a function or
    /// procedure definition.  The argument \p declarationNode is a valid Node
    /// returned from a call to Parser::parseSubroutineDeclaration.  The
    /// endTagStack must hold the expected end tag name.
    void parseSubroutineBody(Node declarationNode);

    bool parseDeclaration();
    bool parseObjectDeclaration();
    bool parseImportDeclaration();

    void parseCarrier();
    void parseAddComponents();

    Node parseStatement();
    Node parseIfStmt();
    Node parseReturnStmt();
    Node parseAssignmentStmt();
    Node parseBlockStmt();
    Node parseWhileStmt();

    Node parseSubroutineKeywordSelection();
    Node parseProcedureCallStatement();

    Node parseExpr();
    Node parsePrimaryExpr();
    Node parseInjExpr();
    Node parsePrjExpr();
    Node parseOperatorExpr();
    Node parseIntegerLiteral();

    Node parseQualifier();
    bool parseType();
    void parseEnumerationList(Node enumeration);
    bool parseIntegerRange(IdentifierInfo *name, Location loc);

    /// \brief Parses a top level construct.  Returns false once all tokens have
    /// been consumed.
    bool parseTopLevelDeclaration();

    /// \brief Returns true if the parser has been driven without seeing an
    /// error and false otherwise.
    bool parseSuccessful() const {
        return errorCount == 0 && lexer.lexSuccessful();
    }

    /// \brief Returns the number of errors seen by this Parser so far.
    unsigned getErrorCount() const {
        return errorCount + lexer.getErrorCount();
    }

private:
    TextProvider   &txtProvider;
    IdentifierPool &idPool;
    ParseClient    &client;
    Diagnostic     &diagnostic;
    Lexer           lexer;

    Lexer::Token token;

    // Maintains a count of the number of errors seen thus far.
    unsigned errorCount;

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
    Lexer::Token  peekToken();
    void setCurrentToken(Lexer::Token &tkn);

    // Ignores the current token and returns the location of the token skipped.
    // This method is used to support the idiom of skipping reserved words and
    // saving the location of the construct.
    Location ignoreToken();

    Lexer::Code currentTokenCode();
    Lexer::Code peekTokenCode();

    bool currentTokenIs(Lexer::Code code);
    bool nextTokenIs(Lexer::Code code);
    bool expectToken(Lexer::Code code);
    bool reduceToken(Lexer::Code code);
    bool requireToken(Lexer::Code code);

    // Matches a right paren assuming that the opening paren has already been
    // consumed, keeping track of nested pairs.  Consumes the closing
    // paren. Note that this method does not save the state of the token stream.
    bool seekCloseParen();

    // Assuming an 'if' token has been consumed, moves the token stream past a
    // matching 'end if' sequence (taking into account inner 'if' expressions.
    bool seekEndIf();

    // Moves the token stream past an 'end loop' sequence, taking into account
    // inner loop statmement.
    bool seekEndLoop();

    bool seekToken(Lexer::Code code);
    bool seekAndConsumeToken(Lexer::Code code);

    bool seekTokens(Lexer::Code code0,
                    Lexer::Code code1 = Lexer::UNUSED_ID,
                    Lexer::Code code2 = Lexer::UNUSED_ID,
                    Lexer::Code code3 = Lexer::UNUSED_ID,
                    Lexer::Code code4 = Lexer::UNUSED_ID,
                    Lexer::Code code5 = Lexer::UNUSED_ID);


    bool seekAndConsumeTokens(Lexer::Code code0,
                              Lexer::Code code1 = Lexer::UNUSED_ID,
                              Lexer::Code code2 = Lexer::UNUSED_ID,
                              Lexer::Code code3 = Lexer::UNUSED_ID,
                              Lexer::Code code4 = Lexer::UNUSED_ID);

    // Seeks a terminating semicolon.  This method skips all semicolons which
    // appear nested within braces.  It trys to find a semicolon at the same
    // syntatic level it was envoked at.  Note that this method does not save
    // the state of the token stream.
    bool seekSemi();

    std::string currentTokenString() {
        return currentToken().getString();
    }

    DiagnosticStream &report(Location loc, diag::Kind kind) {
        ++errorCount;
        SourceLocation sloc = txtProvider.getSourceLocation(loc);
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(SourceLocation sloc, diag::Kind kind) {
        ++errorCount;
        return diagnostic.report(sloc, kind);
    }

    DiagnosticStream &report(diag::Kind kind) {
        ++errorCount;
        SourceLocation sloc = txtProvider.getSourceLocation(currentLocation());
        return diagnostic.report(sloc, kind);
    }

    IdentifierInfo *parseIdentifierInfo();
    IdentifierInfo *parseFunctionIdentifierInfo();

    PM::ParameterMode parseParameterMode();

    // Parses the argument list of a subroutine call.  The current token must be
    // a TKN_LPAREN.  If the parsing succeeds, dst is populated with the nodes
    // for each argument and true is returned.  Otherwise, false is returned and
    // the token stream has been adjusted such that the closing paren of the
    // argument list has been consumed.
    bool parseSubroutineArgumentList(NodeVector &dst);

    Node parseSubroutineDeclaration(Descriptor &desc);

    bool seekEndTag(IdentifierInfo *tag);

    bool seekAndConsumeEndTag(IdentifierInfo *tag);

    // Parses an end tag.  If expectedTag is non-null, parse "end <tag>", otherwise
    // parse "end".  Returns true if tokens were consumed (which can happen when the
    // parse fails due to a missing or unexpected end tag) and false otherwise.
    bool parseEndTag(IdentifierInfo *expectedTag = 0);

    // Returns true if a matching pair of parens "()" is next on the stream of
    // tokens.
    bool unitExprFollows();

    // Returns true if an assignment statement is next on the stream of tokens.
    bool assignmentFollows();

    // Returns true if a keyword selection expression follows:  That is, if
    // the token stream admits an IdentifierInfo followed by a '=>' token.
    bool keywordSelectionFollows();

    // Returns true if a qualification follows on the token stream.
    //
    // More precisely, returns true if the current token is an identifier
    // followed by:
    //
    //   - a double colon;
    //
    //   - a left paren with a matching close paren followed by a double colon.
    bool qualifierFollows();

    // Returns true is a block statement follows on the token stream.
    //
    // More recisely, returns true is the current token is
    //
    //   - an identifier followed by a colon.
    //
    //   - the reserved word `declare'.
    //
    //   - the reserved word `begin'.
    bool blockStmtFollows();

    Node parseExponentialOperator();
    Node parseMultiplicativeOperator();
    Node parseAdditiveOperator();
    Node parseRelationalOperator();

    // Convenience function for obtaining null nodes.
    Node getNullNode() { return client.getNullNode(); }

    // Convenience function for obtaining invalid nodes.
    Node getInvalidNode() { return client.getInvalidNode(); }

    // Converts a character array representing a Comma integer literal into an
    // llvm::APInt.  The bit width of the resulting APInt is always set to the
    // minimal number of bits needed to represent the given number.
    void decimalLiteralToAPInt(const char *start, unsigned length,
                               llvm::APInt &value);
};

} // End comma namespace

#endif
