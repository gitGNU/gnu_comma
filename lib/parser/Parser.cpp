//===-- parser/parser.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008-2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//
//
// This parser is a typical hand written recursive decent parser.
//
// There is always a "current token", and each parse method begins its
// processing using the current token to guide its logic.  Therefore, the rule
// to follow when calling a parse method (or when writing one) is "the current
// token is the next token to be parsed".
//
// Similarly, parse methods leave the stream so that the current token is again
// the next token to be parsed.  Thus, a parse method which consumes exactly one
// token moves the token stream by exactly one token.
//
// As the parser proceeds, callbacks provided by the ParseClient are invoked.
// The parser does not build an AST explicitly -- rather, it formulates calls
// to the client, which in turn could construct an AST, or perform some other
// action.
//
//===----------------------------------------------------------------------===//

#include "comma/basic/Attributes.h"
#include "comma/basic/Pragmas.h"
#include "comma/parser/Parser.h"

#include "llvm/ADT/APInt.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <vector>

using namespace comma;

Parser::Parser(TextProvider &txtProvider, IdentifierPool &idPool,
               ParseClient &client, Diagnostic &diag)
    : txtProvider(txtProvider),
      idPool(idPool),
      client(client),
      diagnostic(diag),
      lexer(txtProvider, diag),
      errorCount(0)
{
    // Mark each identifier which can name an attribute.
    attrib::markAttributeIdentifiers(idPool);

    // Prime the parser by loading in the first token.
    lexer.scan(token);
}

Lexer::Token &Parser::currentToken()
{
    return token;
}

Lexer::Token &Parser::nextToken()
{
    lexer.scan(token);
    return token;
}

Lexer::Token Parser::peekToken()
{
    Lexer::Token tkn;
    lexer.peek(tkn, 0);
    return tkn;
}

Location Parser::ignoreToken()
{
    Location loc = currentLocation();
    nextToken();
    return loc;
}

void Parser::setCurrentToken(Lexer::Token &tkn)
{
    token = tkn;
}

bool Parser::currentTokenIs(Lexer::Code code)
{
    return currentToken().getCode() == code;
}

bool Parser::nextTokenIs(Lexer::Code code)
{
    return peekToken().getCode() == code;
}

Lexer::Code Parser::currentTokenCode()
{
    return currentToken().getCode();
}

Lexer::Code Parser::peekTokenCode()
{
    return peekToken().getCode();
}

bool Parser::expectToken(Lexer::Code code)
{
    if (peekToken().getCode() == code) {
        ignoreToken();
        return true;
    }
    return false;
}

bool Parser::reduceToken(Lexer::Code code)
{
    if (currentTokenIs(code)) {
        ignoreToken();
        return true;
    }
    return false;
}

bool Parser::requireToken(Lexer::Code code)
{
    bool status = reduceToken(code);
    if (!status)
        report(diag::UNEXPECTED_TOKEN_WANTED)
            << currentToken().getString()
            << Lexer::tokenString(code);
    return status;
}

bool Parser::seekToken(Lexer::Code code)
{
    while (!currentTokenIs(Lexer::TKN_EOT)) {
        if (currentTokenIs(code))
            return true;
        else
            ignoreToken();
    }
    return false;
}

bool Parser::seekAndConsumeToken(Lexer::Code code)
{
    bool status = seekToken(code);
    if (status) ignoreToken();
    return status;
}

bool Parser::seekTokens(Lexer::Code code0, Lexer::Code code1,
                        Lexer::Code code2, Lexer::Code code3,
                        Lexer::Code code4, Lexer::Code code5)
{
    Lexer::Code codes[] = { code0, code1, code2, code3, code4, code5 };
    Lexer::Code *end = &codes[6];

    while (!currentTokenIs(Lexer::TKN_EOT))
    {
        if (end != std::find(codes, end, currentTokenCode()))
            return true;
        else
            ignoreToken();
    }
    return false;
}

bool Parser::seekAndConsumeTokens(Lexer::Code code0,
                                  Lexer::Code code1, Lexer::Code code2,
                                  Lexer::Code code3, Lexer::Code code4)
{
    bool status = seekTokens(code0, code1, code2, code3, code4);
    if (status) ignoreToken();
    return status;
}

bool Parser::seekCloseParen()
{
    unsigned depth = 1;

    for (;;) {
        seekTokens(Lexer::TKN_LPAREN, Lexer::TKN_RPAREN);

        switch (currentTokenCode()) {
        default:
            break;

        case Lexer::TKN_LPAREN:
            depth++;
            break;

        case Lexer::TKN_RPAREN:
            depth--;
            if (depth == 0) {
                ignoreToken();
                return true;
            }
            break;

        case Lexer::TKN_EOT:
            return false;
        }

        ignoreToken();
    }
}

bool Parser::seekSemi()
{
    while (seekTokens(Lexer::TKN_LPAREN, Lexer::TKN_SEMI)) {

        if (currentTokenIs(Lexer::TKN_SEMI))
            return true;

        // Otherwise, the current token is an LBRACE.  Dive into the parens and
        // seek the closing token.
        ignoreToken();
        seekCloseParen();
    }
    return false;
}

// This function drives the stream of input tokens looking for an end statement.
// If the end statement is followed by a matching tag, true is returned.
// Otherwise the search continues until a matching end is found or the end of
// the token stream is encountered.  In the latter case, false is returned.
bool Parser::seekEndTag(IdentifierInfo *tag)
{
    while (seekToken(Lexer::TKN_END))
    {
        IdentifierInfo *info = 0;

        if (nextTokenIs(Lexer::TKN_IDENTIFIER)) {
            info = getIdentifierInfo(peekToken());
        }

        if (info == tag)
            return true;
        else
            ignoreToken();
    }
    return false;
}

bool Parser::seekAndConsumeEndTag(IdentifierInfo *tag)
{
    if (seekEndTag(tag)) {
        ignoreToken();                // Ignore 'end'.
        ignoreToken();                // Ignore the tag.
        return true;
    }
    return false;
}

bool Parser::seekEndIf()
{
    unsigned depth = 1;

    while (seekTokens(Lexer::TKN_IF, Lexer::TKN_END)) {
        switch (currentTokenCode()) {

        default:
            return false;

        case Lexer::TKN_IF:
            ignoreToken();
            depth++;
            break;

        case Lexer::TKN_END:
            ignoreToken();
            if (reduceToken(Lexer::TKN_IF)) {
                if (--depth == 0)
                    return true;
            }
        }
    }
    return false;
}

bool Parser::seekEndLoop()
{
    unsigned depth = 1;
    while (seekTokens(Lexer::TKN_WHILE, Lexer::TKN_END)) {
        switch (currentTokenCode()) {
        default:
            return false;

        case Lexer::TKN_WHILE:
            ignoreToken();
            depth++;
            break;

        case Lexer::TKN_END:
            ignoreToken();
            if (reduceToken(Lexer::TKN_LOOP)) {
                if (--depth == 0)
                    return true;
            }
        }
    }
    return false;
}

Location Parser::currentLocation()
{
    return currentToken().getLocation();
}

unsigned Parser::currentLine()
{
    return txtProvider.getLine(currentLocation());
}

unsigned Parser::currentColumn()
{
    return txtProvider.getColumn(currentLocation());
}

IdentifierInfo *Parser::getIdentifierInfo(const Lexer::Token &tkn)
{
    const char *rep = tkn.getRep();
    unsigned length = tkn.getLength();
    IdentifierInfo *info = &idPool.getIdentifierInfo(rep, length);
    return info;
}

bool Parser::unitExprFollows()
{
    return currentTokenIs(Lexer::TKN_LPAREN) && nextTokenIs(Lexer::TKN_RPAREN);
}

bool Parser::assignmentFollows()
{
    Lexer::Token savedToken = currentToken();
    lexer.beginExcursion();
    seekNameEnd();
    bool status = currentTokenIs(Lexer::TKN_ASSIGN);
    lexer.endExcursion();
    setCurrentToken(savedToken);
    return status;
}

bool Parser::keywordSelectionFollows()
{
    return currentTokenIs(Lexer::TKN_IDENTIFIER)
        && nextTokenIs(Lexer::TKN_RDARROW);
}

bool Parser::selectedComponentFollows()
{
    bool status = false;

    if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        switch (peekTokenCode()) {

        default:
            break;

        case Lexer::TKN_DOT:
            status = true;
            break;

        case Lexer::TKN_LPAREN: {
            Lexer::Token savedToken = currentToken();
            lexer.beginExcursion();
            ignoreToken();      // Ignore the identifier.
            do {
                ignoreToken();  // Ignore the left paren.
                seekCloseParen();
            } while (currentTokenIs(Lexer::TKN_LPAREN));
            status = currentTokenIs(Lexer::TKN_DOT);
            lexer.endExcursion();
            setCurrentToken(savedToken);
            break;
        }
        }
    }
    return status;
}

Parser::AggregateKind Parser::aggregateFollows()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    AggregateKind result = NOT_AN_AGGREGATE;
    Lexer::Token savedToken = currentToken();

    lexer.beginExcursion();
    ignoreToken();              // Ignore the left paren.

SEEK:
    if (seekTokens(Lexer::TKN_LPAREN,
                   Lexer::TKN_COMMA, Lexer::TKN_OTHERS,
                   Lexer::TKN_RDARROW, Lexer::TKN_RPAREN)) {

        Lexer::Code code = currentTokenCode();
        if (code == Lexer::TKN_COMMA)
            result = POSITIONAL_AGGREGATE;
        else if (code == Lexer::TKN_RDARROW)
            result = KEYED_AGGREGATE;
        else {
            if (code == Lexer::TKN_LPAREN) {
                ignoreToken();
                if (seekCloseParen())
                    goto SEEK;
            }
        }
    }

    lexer.endExcursion();
    setCurrentToken(savedToken);
    return result;
}

bool Parser::blockStmtFollows()
{
    switch (currentTokenCode()) {

    default:
        return false;

    case Lexer::TKN_IDENTIFIER:
        return nextTokenIs(Lexer::TKN_COLON);

    case Lexer::TKN_DECLARE:
    case Lexer::TKN_BEGIN:
        return true;
    }
}

IdentifierInfo *Parser::parseIdentifierInfo()
{
    IdentifierInfo *info;

    switch (currentTokenCode()) {
    case Lexer::TKN_IDENTIFIER:
        info = getIdentifierInfo(currentToken());
        ignoreToken();
        break;

    case Lexer::TKN_EOT:
        report(diag::PREMATURE_EOS);
        info = 0;
        break;

    default:
        report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
        info = 0;
    }
    return info;
}

IdentifierInfo *Parser::parseFunctionIdentifierInfo()
{
    IdentifierInfo *info;

    if (Lexer::isFunctionGlyph(currentToken())) {
        const char *rep = Lexer::tokenString(currentTokenCode());
        info = &idPool.getIdentifierInfo(rep);
        ignoreToken();
    }
    else
        info = parseIdentifierInfo();
    return info;
}

IdentifierInfo *Parser::parseCharacter()
{
    if (currentTokenIs(Lexer::TKN_CHARACTER)) {
        IdentifierInfo *info = getIdentifierInfo(currentToken());
        ignoreToken();
        return info;
    }
    else {
        report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
        return 0;
    }
}

IdentifierInfo *Parser::parseIdentifierOrCharacter()
{
    if (currentTokenIs(Lexer::TKN_IDENTIFIER))
        return parseIdentifierInfo();
    else
        return parseCharacter();
}

// Parses an end tag.  If expectedTag is non-null, parse "end <tag>", otherwise
// parse "end".  Returns true if tokens were consumed (which can happen when the
// parse fails due to a missing or unexpected end tag) and false otherwise.
bool Parser::parseEndTag(IdentifierInfo *expectedTag)
{
    Location tagLoc;
    IdentifierInfo *tag;

    if (requireToken(Lexer::TKN_END)) {
        if (expectedTag) {
            if (currentTokenIs(Lexer::TKN_SEMI))
                report(diag::EXPECTED_END_TAG) << expectedTag;
            else {
                tagLoc = currentLocation();
                tag = parseFunctionIdentifierInfo();
                if (tag && tag != expectedTag)
                    report(tagLoc, diag::EXPECTED_END_TAG) << expectedTag;
            }
        }
        else if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
            // FIXME:  The above test is not general enough, since we could have
            // operator tokens (TKN_PLUS, TKN_STAR, etc) labeling an "end".
            tagLoc = currentLocation();
            tag = parseIdentifierInfo();
            report(tagLoc, diag::UNEXPECTED_END_TAG) << tag;
        }
        return true;
    }
    return false;
}

void Parser::parseGenericFormalParams()
{
    assert(currentTokenIs(Lexer::TKN_GENERIC));
    ignoreToken();

    client.beginGenericFormals();
    for ( ;; ) {
        switch (currentTokenCode()) {

        default:
            report(diag::UNEXPECTED_TOKEN) << currentTokenString();
            if (seekTokens(Lexer::TKN_ABSTRACT,
                           Lexer::TKN_DOMAIN, Lexer::TKN_SIGNATURE)) {
                if (currentTokenIs(Lexer::TKN_ABSTRACT))
                    continue;
            }
            client.endGenericFormals();
            return;

        case Lexer::TKN_ABSTRACT:
            parseGenericFormalDomain();
            break;

        case Lexer::TKN_DOMAIN:
        case Lexer::TKN_SIGNATURE:
            client.endGenericFormals();
            return;
        }
    }
}

void Parser::parseGenericFormalDomain()
{
    assert(currentTokenIs(Lexer::TKN_ABSTRACT));
    ignoreToken();

    if (!requireToken(Lexer::TKN_DOMAIN)) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    if (reduceToken(Lexer::TKN_IS)) {
        Node sig = parseName();
        if (sig.isValid())
            client.acceptFormalDomain(name, loc, sig);
    }
    else
        client.acceptFormalDomain(name, loc, getNullNode());

    requireToken(Lexer::TKN_SEMI);
}

void Parser::parseSignatureProfile()
{
    client.beginSignatureProfile();

    if (currentTokenIs(Lexer::TKN_IS))
        parseSupersignatureProfile();

    if (currentTokenIs(Lexer::TKN_WITH))
        parseWithProfile();

    client.endSignatureProfile();
}

// Parses a sequence of super-signatures in a 'with' expression.
void Parser::parseSupersignatureProfile()
{
    assert(currentTokenIs(Lexer::TKN_IS));
    ignoreToken();

    do {
        Node super = parseName();

        if (super.isValid())
            client.acceptSupersignature(super);
        else {
            seekTokens(Lexer::TKN_AND, Lexer::TKN_ADD,
                       Lexer::TKN_WITH, Lexer::TKN_END);
        }
    } while (reduceToken(Lexer::TKN_AND));
}

void Parser::parseWithProfile()
{
    assert(currentTokenIs(Lexer::TKN_WITH));
    ignoreToken();

    bool status = false;

    for (;;) {
        switch (currentTokenCode()) {
        default:
            return;

        case Lexer::TKN_FUNCTION:
            status = parseFunctionDeclaration(true).isValid();
            break;

        case Lexer::TKN_PROCEDURE:
            status = parseProcedureDeclaration(true).isValid();
            break;

        case Lexer::TKN_TYPE:
            status = parseType();
            break;

        case Lexer::TKN_SUBTYPE:
            status = parseSubtype();
            break;
        }

        if (!status)
            seekTokens(Lexer::TKN_FUNCTION, Lexer::TKN_PROCEDURE,
                       Lexer::TKN_TYPE,     Lexer::TKN_SEMI,
                       Lexer::TKN_END,      Lexer::TKN_ADD);

        requireToken(Lexer::TKN_SEMI);
    }
}

void Parser::parseCarrier()
{
    assert(currentTokenIs(Lexer::TKN_CARRIER));

    Location loc = ignoreToken();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    if (!requireToken(Lexer::TKN_IS)) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    Node type = parseName();

    if (type.isInvalid()) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    client.acceptCarrier(name, loc, type);
}

void Parser::parseAddComponents()
{
    client.beginAddExpression();

    for (;;) {
        switch (currentTokenCode()) {
        default:
            client.endAddExpression();
            return;

        case Lexer::TKN_FUNCTION:
            parseFunctionDeclOrDefinition();
            break;

        case Lexer::TKN_PROCEDURE:
            parseProcedureDeclOrDefinition();
            break;

        case Lexer::TKN_IMPORT:
            parseImportDeclaration();
            break;

        case Lexer::TKN_CARRIER:
            parseCarrier();
            break;

        case Lexer::TKN_TYPE:
            parseType();
            break;

        case Lexer::TKN_SUBTYPE:
            parseSubtype();
            break;

        case Lexer::TKN_PRAGMA:
            parseDeclarationPragma();
            break;
        }

        requireToken(Lexer::TKN_SEMI);
    }
}

void Parser::parseModel()
{
    bool parsingDomain = false;
    IdentifierInfo *name = 0;

    client.beginCapsule();

    if (currentTokenIs(Lexer::TKN_GENERIC))
        parseGenericFormalParams();

    if (reduceToken(Lexer::TKN_DOMAIN)) {
        Location loc = currentLocation();
        if (!(name = parseIdentifierInfo()))
            return;
        client.beginDomainDecl(name, loc);
        parsingDomain = true;
    }
    else if (reduceToken(Lexer::TKN_SIGNATURE)) {
        Location loc = currentLocation();
        if (!(name = parseIdentifierInfo()))
            return;
        client.beginSignatureDecl(name, loc);
    }
    else {
        assert(false && "Bad token for this production!");
        return;
    }

    if (currentTokenIs(Lexer::TKN_IS) || currentTokenIs(Lexer::TKN_WITH))
        parseSignatureProfile();

    if (parsingDomain && reduceToken(Lexer::TKN_ADD))
        parseAddComponents();

    client.endCapsule();

    // Consume and verify the end tag.  On failure seek the next top level form.
    if (!parseEndTag(name))
        seekTokens(Lexer::TKN_SIGNATURE, Lexer::TKN_DOMAIN);
    else
        requireToken(Lexer::TKN_SEMI);
}

// Parses an "in", "out" or "in out" parameter mode specification.  If no such
// specification is available on the stream MODE_DEFAULT is returned.  A common
// mistake is to find "out in" instead of "in out".  In this case, we simply
// issue a diagnostic and return MODE_IN_OUT.
PM::ParameterMode Parser::parseParameterMode()
{
    PM::ParameterMode mode = PM::MODE_DEFAULT;

    if (reduceToken(Lexer::TKN_IN)) {
        if (reduceToken(Lexer::TKN_OUT))
            mode = PM::MODE_IN_OUT;
        else
            mode = PM::MODE_IN;
    }
    else if (reduceToken(Lexer::TKN_OUT)) {
        if (currentTokenIs(Lexer::TKN_IN)) {
            report(diag::OUT_IN_PARAMETER_MODE);
            ignoreToken();
            mode = PM::MODE_IN_OUT;
        }
        else
            mode = PM::MODE_OUT;
    }
    return mode;
}

bool Parser::parseSubroutineParameter()
{
    IdentifierInfo *formal;
    Location location;
    PM::ParameterMode mode;

    location = currentLocation();
    formal = parseIdentifierInfo();

    if (!formal) return false;

    if (!requireToken(Lexer::TKN_COLON)) return false;

    mode = parseParameterMode();
    Node type = parseName();
    if (type.isInvalid()) return false;

    client.acceptSubroutineParameter(formal, location, type, mode);
    return true;
}

void Parser::parseSubroutineParameters()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    // Check that we do not have an empty parameter list.
    if (unitExprFollows()) {
        report(diag::EMPTY_PARAMS);

        // Consume the opening and closing parens.
        ignoreToken();
        ignoreToken();
        return;
    }

    // Consume the opening paren.
    ignoreToken();

    for (;;) {
        if (!parseSubroutineParameter())
            seekTokens(Lexer::TKN_SEMI, Lexer::TKN_RPAREN);

        switch (currentTokenCode()) {

        default:
            // An unexpected token.  Abort processing of the parameter list and
            // seek a closing paren.
            report(diag::UNEXPECTED_TOKEN)  << currentTokenString();
            if (seekCloseParen()) ignoreToken();
            return;

        case Lexer::TKN_COMMA:
            // Using a comma instead of a semicolon is a common mistake.  Issue
            // a diagnostic and continue processing as though a semi was found.
            report(diag::UNEXPECTED_TOKEN_WANTED) << "," << ";";
            ignoreToken();
            break;

        case Lexer::TKN_SEMI:
            // OK, process the next parameter.
            ignoreToken();
            break;

        case Lexer::TKN_RPAREN:
            // The parameter list is complete.  Consume the paren and return.
            ignoreToken();
            return;
        }
    }
}

Node Parser::parseFunctionDeclaration(bool parsingSignatureProfile)
{
    assert(currentTokenIs(Lexer::TKN_FUNCTION));
    ignoreToken();

    Location location = currentLocation();
    IdentifierInfo *name = parseFunctionIdentifierInfo();

    if (!name)
        return getInvalidNode();

    client.beginFunctionDeclaration(name, location);

    if (currentTokenIs(Lexer::TKN_LPAREN))
        parseSubroutineParameters();

    Node returnNode = getNullNode();
    if (reduceToken(Lexer::TKN_RETURN)) {
        returnNode = parseName();
        if (returnNode.isInvalid()) {
            seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
            returnNode = getNullNode();
        }
    }
    else
        report(diag::MISSING_RETURN_AFTER_FUNCTION);

    client.acceptFunctionReturnType(returnNode);

    bool bodyFollows = currentTokenIs(Lexer::TKN_IS);

    // FIXME: We should model the parser state with more than a tag stack.
    if (bodyFollows)
        endTagStack.push(EndTagEntry(NAMED_TAG, location, name));

    return client.endSubroutineDeclaration(bodyFollows);
}

Node Parser::parseProcedureDeclaration(bool parsingSignatureProfile)
{
    assert(currentTokenIs(Lexer::TKN_PROCEDURE));
    ignoreToken();

    Location location = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name)
        return getInvalidNode();

    client.beginProcedureDeclaration(name, location);

    if (currentTokenIs(Lexer::TKN_LPAREN))
        parseSubroutineParameters();

    if (currentTokenIs(Lexer::TKN_RETURN)) {
        report(diag::RETURN_AFTER_PROCEDURE);
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
    }

    bool bodyFollows = currentTokenIs(Lexer::TKN_IS);

    // FIXME: We should model the parser state with more than a tag stack.
    if (bodyFollows)
        endTagStack.push(EndTagEntry(NAMED_TAG, location, name));

    return client.endSubroutineDeclaration(bodyFollows);
}

void Parser::parseSubroutineBody(Node declarationNode)
{
    client.beginSubroutineDefinition(declarationNode);

    while (!currentTokenIs(Lexer::TKN_BEGIN) &&
           !currentTokenIs(Lexer::TKN_EOT)) {
        parseDeclaration();
        requireToken(Lexer::TKN_SEMI);
    }

    requireToken(Lexer::TKN_BEGIN);

    while (!currentTokenIs(Lexer::TKN_END) &&
           !currentTokenIs(Lexer::TKN_EOT)) {
        Node stmt = parseStatement();
        if (stmt.isValid())
            client.acceptSubroutineStmt(stmt);
    }

    EndTagEntry tagEntry = endTagStack.top();
    assert(tagEntry.kind == NAMED_TAG && "Inconsistent end tag stack!");

    endTagStack.pop();
    parseEndTag(tagEntry.tag);
    client.endSubroutineDefinition();
}

void Parser::parseFunctionDeclOrDefinition()
{
    Node decl = parseFunctionDeclaration();

    if (decl.isInvalid()) {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
        if (currentTokenIs(Lexer::TKN_IS)) {
            EndTagEntry tagEntry = endTagStack.top();
            assert(tagEntry.kind == NAMED_TAG && "Inconsistent end tag stack!");
            endTagStack.pop();
            seekAndConsumeEndTag(tagEntry.tag);
        }
        return;
    }

    if (reduceToken(Lexer::TKN_IS))
        parseSubroutineBody(decl);
    return;
}

void Parser::parseProcedureDeclOrDefinition()
{
    Node decl = parseProcedureDeclaration();

    if (decl.isInvalid()) {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
        if (currentTokenIs(Lexer::TKN_IS)) {
            EndTagEntry tagEntry = endTagStack.top();
            assert(tagEntry.kind == NAMED_TAG && "Inconsistent end tag stack!");
            endTagStack.pop();
            seekAndConsumeEndTag(tagEntry.tag);
        }
        return;
    }

    if (reduceToken(Lexer::TKN_IS))
        parseSubroutineBody(decl);
    return;
}

bool Parser::parseDeclaration()
{
    switch (currentTokenCode()) {
    default:
        report(diag::UNEXPECTED_TOKEN) << currentTokenString();
        seekAndConsumeToken(Lexer::TKN_SEMI);
        return false;

    case Lexer::TKN_IDENTIFIER:
        return parseObjectDeclaration();

    case Lexer::TKN_FUNCTION:
        return parseFunctionDeclaration().isValid();

    case Lexer::TKN_PROCEDURE:
        return parseProcedureDeclaration().isValid();

    case Lexer::TKN_IMPORT:
        return parseImportDeclaration();
    }
}

bool Parser::parseObjectDeclaration()
{
    IdentifierInfo *id;
    Location loc;

    assert(currentTokenIs(Lexer::TKN_IDENTIFIER));

    loc = currentLocation();
    id = parseIdentifierInfo();

    if (!(id && requireToken(Lexer::TKN_COLON))) {
        seekAndConsumeToken(Lexer::TKN_SEMI);
        return false;
    }

    Node type = parseName();

    if (type.isValid()) {
        Node init = getNullNode();
        if (reduceToken(Lexer::TKN_ASSIGN))
            init = parseExpr();
        if (init.isValid()) {
            client.acceptObjectDeclaration(loc, id, type, init);
            return true;
        }
    }
    seekToken(Lexer::TKN_SEMI);
    return false;
}

bool Parser::parseImportDeclaration()
{
    assert(currentTokenIs(Lexer::TKN_IMPORT));
    ignoreToken();

    Node importedType = parseName();

    if (importedType.isValid()) {
        client.acceptImportDeclaration(importedType);
        return true;
    }
    return false;
}

bool Parser::parseType()
{
    assert(currentTokenIs(Lexer::TKN_TYPE));
    ignoreToken();

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name || !requireToken(Lexer::TKN_IS))
        return false;

    switch (currentTokenCode()) {

    default:
        report(diag::UNEXPECTED_TOKEN_WANTED)
            << currentTokenString() << "(" << "range" << "array";
        break;

    case Lexer::TKN_LPAREN: {
        // We must have an enumeration type.
        client.beginEnumeration(name, loc);
        parseEnumerationList();
        client.endEnumeration();
        return true;
    }

    case Lexer::TKN_RANGE:
        return parseIntegerRange(name, loc);

    case Lexer::TKN_ARRAY:
        return parseArrayTypeDecl(name, loc);
    }

    return false;
}

bool Parser::parseSubtype()
{
    assert(currentTokenIs(Lexer::TKN_SUBTYPE));
    ignoreToken();

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name || !requireToken(Lexer::TKN_IS)) {
        seekSemi();
        return false;
    }

    Node subtype = parseName();

    if (subtype.isInvalid()) {
        seekSemi();
        return false;
    }

    if (currentTokenIs(Lexer::TKN_SEMI)) {
        client.acceptSubtypeDecl(name, loc, subtype);
        return true;
    }

    // The only kind of subtype constraints we contend with at the moment are
    // range constraints.
    Node low = parseExpr();
    if (low.isInvalid() or !requireToken(Lexer::TKN_DDOT)) {
        seekSemi();
        return false;
    }

    Node high = parseExpr();
    if (high.isInvalid()) {
        seekSemi();
        return false;
    }

    client.acceptRangedSubtypeDecl(name, loc, subtype, low, high);
    return true;
}

void Parser::parseEnumerationList()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    Location loc = currentLocation();
    ignoreToken();

    // Diagnose empty enumeration lists.
    if (reduceToken(Lexer::TKN_RPAREN)) {
        report(loc, diag::EMPTY_ENUMERATION);
        return;
    }

    do {
        Location loc = currentLocation();
        if (currentTokenIs(Lexer::TKN_CHARACTER)) {
            IdentifierInfo *name = parseCharacter();
            client.acceptEnumerationCharacter(name, loc);
        }
        else {
            IdentifierInfo *name = parseIdentifierInfo();

            if (!name) {
                seekCloseParen();
                return;
            }
            client.acceptEnumerationIdentifier(name, loc);
        }
    } while (reduceToken(Lexer::TKN_COMMA));

    requireToken(Lexer::TKN_RPAREN);
}

bool Parser::parseIntegerRange(IdentifierInfo *name, Location loc)
{
    assert(currentTokenIs(Lexer::TKN_RANGE));
    ignoreToken();

    Node low = parseExpr();
    if (low.isInvalid() or !requireToken(Lexer::TKN_DDOT)) {
        seekSemi();
        return false;
    }

    Node high = parseExpr();
    if (high.isInvalid()) {
        seekSemi();
        return false;
    }

    client.acceptIntegerTypeDecl(name, loc, low, high);
    return true;
}

void Parser::parseArrayIndexProfile()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    ignoreToken();

    // Diagnose empty index profiles.
    if (reduceToken(Lexer::TKN_RPAREN)) {
        report(diag::EMPTY_ARRAY_TYPE_INDICES);
        return;
    }

    bool parsedIndex = false;   // True when we have parsed an index.
    bool isConstrained = false; // True when we have seen a constrained index.
    do {
        Location loc = currentLocation();
        Node index = parseName();
        if (index.isInvalid())
            continue;

        if (reduceToken(Lexer::TKN_RANGE)) {
            if (reduceToken(Lexer::TKN_DIAMOND)) {
                // If we have already parsed an index, ensure that it was
                // constrained.
                if (parsedIndex && !isConstrained)
                    report(loc, diag::EXPECTED_CONSTRAINED_ARRAY_INDEX);
                else {
                    client.acceptUnconstrainedArrayIndex(index);
                    isConstrained = true;
                    parsedIndex = true;
                }
            }
            else {
                // Only unconstrained array indexes are supported ATM.
                report(diag::UNEXPECTED_TOKEN_WANTED)
                    << currentTokenString() << "<>";
                seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
            }
        }
        else {
            // Check that an unconstrained index is not expected.
            if (parsedIndex && isConstrained)
                report(loc, diag::EXPECTED_UNCONSTRAINED_ARRAY_INDEX);
            else {
                client.acceptArrayIndex(index);
                parsedIndex = true;
            }
        }
    } while (reduceToken(Lexer::TKN_COMMA));

    if (!requireToken(Lexer::TKN_RPAREN))
        seekCloseParen();
}

bool Parser::parseArrayTypeDecl(IdentifierInfo *name, Location loc)
{
    assert(currentTokenIs(Lexer::TKN_ARRAY));
    ignoreToken();

    client.beginArray(name, loc);

    if (!currentTokenIs(Lexer::TKN_LPAREN)) {
        client.endArray();
        return false;
    }

    parseArrayIndexProfile();

    if (!reduceToken(Lexer::TKN_OF)) {
        client.endArray();
        return false;
    }

    Node component = parseName();
    if (component.isValid()) {
        client.acceptArrayComponent(component);
        client.endArray();
        return true;
    }

    client.endArray();
    return false;
}

bool Parser::parseTopLevelDeclaration()
{
    for (;;) {
        switch (currentTokenCode()) {
        case Lexer::TKN_SIGNATURE:
        case Lexer::TKN_DOMAIN:
        case Lexer::TKN_GENERIC:
            parseModel();
            return true;

        case Lexer::TKN_EOT:
            return false;
            break;

        default:
            // An invalid token was found at top level. Do not try to recover.
            report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
            return false;
        }
    }
}

// Converts a character array representing a Comma integer literal into an
// llvm::APInt.  The bit width of the resulting APInt is always set to the
// minimal number of bits needed to represent the given number.
void Parser::decimalLiteralToAPInt(const char *start, unsigned length,
                                   llvm::APInt &value)
{
    std::string digits;
    for (const char *cursor = start; cursor != start + length; ++cursor) {
        char ch = *cursor;
        if (ch != '_')
            digits.push_back(ch);
    }
    assert(!digits.empty() && "Empty string literal!");

    // Get the binary value and adjust the number of bits to an accurate width.
    unsigned numBits = llvm::APInt::getBitsNeeded(digits, 10);
    value = llvm::APInt(numBits, digits, 10);
    if (value == 0)
        numBits = 1;
    else
        numBits = value.getActiveBits();
    value.zextOrTrunc(numBits);
}

void Parser::parseDeclarationPragma()
{
    assert(currentTokenIs(Lexer::TKN_PRAGMA));
    ignoreToken();

    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name)
        return;

    llvm::StringRef ref(name->getString());
    pragma::PragmaID ID = pragma::getPragmaID(ref);

    if (ID == pragma::UNKNOWN_PRAGMA) {
        report(loc, diag::UNKNOWN_PRAGMA) << name;
        return;
    }

    // Currently, the only pragma accepted in a declaration context is Import.
    // When the set of valid pragmas expands, special parsers will be written to
    // parse the arguments.
    switch (ID) {
    default:
        report(loc, diag::INVALID_PRAGMA_CONTEXT) << name;
        break;

    case pragma::Import:
        parsePragmaImport(loc);
        break;
    }
}

void Parser::parsePragmaImport(Location pragmaLoc)
{
    if (!requireToken(Lexer::TKN_LPAREN))
        return;

    // The first argument is an identifier naming the import convention.  The
    // parser does not know anything about convention names.
    Location conventionLoc = currentLocation();
    IdentifierInfo *conventionName = parseIdentifierInfo();
    if (!conventionName || !requireToken(Lexer::TKN_COMMA)) {
        seekCloseParen();
        return;
    }

    // The second argument is the name of the local declaration corresponding to
    // the imported entity.
    Location entityLoc = currentLocation();
    IdentifierInfo *entityName = parseFunctionIdentifierInfo();
    if (!entityName || !requireToken(Lexer::TKN_COMMA)) {
        seekCloseParen();
        return;
    }

    // Finally, the external name.  This is a general expression.
    Node externalName = parseExpr();
    if (externalName.isInvalid() || !requireToken(Lexer::TKN_RPAREN)) {
        seekCloseParen();
        return;
    }

    client.acceptPragmaImport(pragmaLoc,
                              conventionName, conventionLoc,
                              entityName, entityLoc, externalName);
}
