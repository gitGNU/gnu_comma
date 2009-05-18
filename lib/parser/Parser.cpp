//===-- parser/parser.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2008, Stephen Wilson
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

#include "comma/parser/Parser.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace comma;

Parser::Parser(TextProvider   &txtProvider,
               IdentifierPool &idPool,
               ParseClient    &client,
               Diagnostic     &diag)
    : txtProvider(txtProvider),
      idPool(idPool),
      client(client),
      diagnostic(diag),
      lexer(txtProvider, diag),
      errorCount(0)
{
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
    Lexer::Code *end    = &codes[6];

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
    const char     *rep    = tkn.getRep();
    unsigned        length = tkn.getLength();
    IdentifierInfo *info   = &idPool.getIdentifierInfo(rep, length);
    return info;
}

bool Parser::unitExprFollows()
{
    return currentTokenIs(Lexer::TKN_LPAREN) && nextTokenIs(Lexer::TKN_RPAREN);
}

bool Parser::assignmentFollows()
{
    return currentTokenIs(Lexer::TKN_IDENTIFIER) &&
        nextTokenIs(Lexer::TKN_ASSIGN);
}

bool Parser::keywordSelectionFollows()
{
    return currentTokenIs(Lexer::TKN_IDENTIFIER)
        && nextTokenIs(Lexer::TKN_RDARROW);
}

bool Parser::qualifierFollows()
{
    bool status = false;

    if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        switch (peekTokenCode()) {

        default:
            break;

        case Lexer::TKN_DCOLON:
            status = true;
            break;

        case Lexer::TKN_LPAREN: {
            Lexer::Token savedToken = currentToken();
            lexer.beginExcursion();
            ignoreToken();      // ignore the identifier.
            ignoreToken();      // ignore the left paren.
            if (seekCloseParen())
                status = reduceToken(Lexer::TKN_DCOLON);
            lexer.endExcursion();
            setCurrentToken(savedToken);
            break;
        }
        }
    }
    return status;
}

Node Parser::parseQualifier()
{
    Node     qualifier = getNullNode();
    Location  location = currentLocation();
    Node qualifierType = parseModelApplication(qualifier);

    if (qualifierType.isInvalid()) {
        do {
            seekAndConsumeToken(Lexer::TKN_DCOLON);
        } while (qualifierFollows());
        return getInvalidNode();
    }

    if (reduceToken(Lexer::TKN_DCOLON)) {

        qualifier = client.acceptQualifier(qualifierType, location);

        while (qualifierFollows()) {
            location      = currentLocation();
            qualifierType = parseModelApplication(qualifier);

            if (qualifierType.isInvalid()) {
                do {
                    seekAndConsumeToken(Lexer::TKN_DCOLON);
                } while (qualifierFollows());
                return getInvalidNode();
            }
            else if (qualifier.isValid()) {
                assert(currentTokenIs(Lexer::TKN_DCOLON));
                ignoreToken();
                qualifier = client.acceptNestedQualifier(qualifier,
                                                         qualifierType,
                                                         location);
            }
        }
        return qualifier;
    }
    return getInvalidNode();
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

// Parses an end tag.  If expectedTag is non-null, parse "end <tag>", otherwise
// parse "end".  Returns true if tokens were consumed (which can happen when the
// parse fails due to a missing or unexpected end tag) and false otherwise.
bool Parser::parseEndTag(IdentifierInfo *expectedTag)
{
    Location        tagLoc;
    IdentifierInfo *tag;

    if (requireToken(Lexer::TKN_END)) {
        if (expectedTag) {
            if (currentTokenIs(Lexer::TKN_SEMI))
                report(diag::EXPECTED_END_TAG) << expectedTag;
            else {
                tagLoc = currentLocation();
                tag    = parseIdentifierInfo();
                if (tag && tag != expectedTag)
                    report(tagLoc, diag::EXPECTED_END_TAG) << expectedTag;
            }
        }
        else if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
            tagLoc = currentLocation();
            tag    = parseIdentifierInfo();
            report(tagLoc, diag::UNEXPECTED_END_TAG) << tag;
        }
        return true;
    }
    return false;
}

// Parses a formal parameter of a model: "id : type".  Passes the parsed type
// off to the type checker.
void Parser::parseModelParameter(Descriptor &desc)
{
    IdentifierInfo *formal;
    Location        loc;

    loc = currentLocation();
    formal = parseIdentifierInfo();

    if (formal && requireToken(Lexer::TKN_COLON)) {
        Node type = parseModelInstantiation();
        if (type.isValid()) {
            Node param = client.acceptModelParameter(desc, formal, type, loc);
            if (param.isValid())
                desc.addParam(param);
            return;
        }
    }
    seekTokens(Lexer::TKN_SEMI, Lexer::TKN_RPAREN);
    return;
}

// Assumes the current token is a left paren begining a model parameter list.
// Parses the list and serves it to the type checker.
void Parser::parseModelParameterization(Descriptor &desc)
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    ignoreToken();

    // We do not permit empty parameter lists.
    if (reduceToken(Lexer::TKN_RPAREN)) {
        report(diag::EMPTY_PARAMS);
        return;
    }

    do {
        parseModelParameter(desc);
    } while (reduceToken(Lexer::TKN_SEMI));

    requireToken(Lexer::TKN_RPAREN);
}

void Parser::parseWithExpression()
{
    assert(currentTokenIs(Lexer::TKN_WITH));
    ignoreToken();

    client.beginWithExpression();
    parseWithSupersignatures();
    parseWithDeclarations();
    client.endWithExpression();
}

// Parses a sequence of super-signatures in a 'with' expression.
void Parser::parseWithSupersignatures()
{
    while (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        Location loc = currentLocation();
        Node   super = parseModelInstantiation();
        if (super.isValid()) {
            requireToken(Lexer::TKN_SEMI);
            client.acceptWithSupersignature(super, loc);
        }
        else {
            seekTokens(Lexer::TKN_SEMI, Lexer::TKN_FUNCTION,
                       Lexer::TKN_ADD,  Lexer::TKN_END);
            // If we skipped to a semicolon, reduce it an attempt to continue
            // any remaining super signature expressions.
            reduceToken(Lexer::TKN_SEMI);
        }
    }
}

void Parser::parseWithDeclarations()
{
    bool status = false;

    for (;;) {
        switch (currentTokenCode()) {
        default:
            return;

        case Lexer::TKN_FUNCTION:
            status = parseFunctionDeclaration().isValid();
            break;

        case Lexer::TKN_PROCEDURE:
            status = parseProcedureDeclaration().isValid();
            break;

        case Lexer::TKN_TYPE:
            status = parseType().isValid();
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
    IdentifierInfo *name;
    Location        loc;

    assert(currentTokenIs(Lexer::TKN_CARRIER));

    loc  = ignoreToken();
    name = parseIdentifierInfo();

    if (!name) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    if (!requireToken(Lexer::TKN_IS)) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    Node type = parseModelInstantiation();

    if (type.isInvalid()) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    client.acceptCarrier(name, type, loc);
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
        }

        requireToken(Lexer::TKN_SEMI);
    }
}

void Parser::parseModelDeclaration(Descriptor &desc)
{
    IdentifierInfo *name;
    Location        location;

    assert(currentTokenIs(Lexer::TKN_SIGNATURE) ||
           currentTokenIs(Lexer::TKN_DOMAIN));

    switch (currentTokenCode()) {
    default:
        assert(false);
        break;

    case Lexer::TKN_SIGNATURE:
        ignoreToken();
        desc.initialize(Descriptor::DESC_Signature);
        break;

    case Lexer::TKN_DOMAIN:
        ignoreToken();
        desc.initialize(Descriptor::DESC_Domain);
    }

    location = currentLocation();
    name     = parseIdentifierInfo();

    // If we cannot even parse the models name, we do not even attempt a
    // recovery sice we would not even know what end tag to look for.
    if (!name) return;

    desc.setIdentifier(name, location);
    client.beginModelDeclaration(desc);

    if (currentTokenIs(Lexer::TKN_LPAREN))
        parseModelParameterization(desc);

    client.acceptModelDeclaration(desc);
}

void Parser::parseModel()
{
    Descriptor desc(&client);

    parseModelDeclaration(desc);

    if (currentTokenIs(Lexer::TKN_WITH))
        parseWithExpression();

    if (desc.isDomainDescriptor() && reduceToken(Lexer::TKN_ADD))
        parseAddComponents();

    // Consume and verify the end tag.  On failure seek the next top level form.
    if (!parseEndTag(desc.getIdInfo()))
        seekTokens(Lexer::TKN_SIGNATURE, Lexer::TKN_DOMAIN);

    requireToken(Lexer::TKN_SEMI);
    client.endModelDefinition();
}

Node Parser::parseModelApplication(Node qualNode)
{
    Location         loc = currentLocation();
    IdentifierInfo *info = parseIdentifierInfo();
    if (!info) return getInvalidNode();

    // Empty parameter lists for types are not allowed.  If the type checker
    // accepts the non-parameterized form, then continue -- otherwise we
    // complain ourselves.
    if (unitExprFollows()) {
        Node type = client.acceptTypeName(info, loc, qualNode);
        if (type.isValid()) report(loc, diag::EMPTY_PARAMS);
        return type;
    }

    if (reduceToken(Lexer::TKN_LPAREN)) {
        NodeVector     arguments;
        LocationVector argumentLocs;
        IdInfoVector   keywords;
        LocationVector keywordLocs;
        bool allOK = true;

        do {
            Location loc = currentLocation();

            if (keywordSelectionFollows()) {
                keywords.push_back(parseIdentifierInfo());
                keywordLocs.push_back(loc);
                loc = ignoreToken();
            }
            else if (!keywords.empty()) {
                // We have already parsed a keyword.
                report(loc, diag::POSITIONAL_FOLLOWING_SELECTED_PARAMETER);
                allOK = false;
            }

            Node argument = parseModelInstantiation();
            if (argument.isValid()) {
                arguments.push_back(argument);
                argumentLocs.push_back(loc);
            }
            else
                allOK = false;

        } while (reduceToken(Lexer::TKN_COMMA));
        requireToken(Lexer::TKN_RPAREN);

        // Do not attempt to form the application unless all of the
        // arguments are valid.
        if (allOK) {
            Location        *argLocs = &argumentLocs[0];
            IdentifierInfo **keys    = &keywords[0];
            Location        *keyLocs = &keywordLocs[0];
            unsigned         numKeys = keywords.size();
            return client.acceptTypeApplication(
                info, arguments, argLocs,
                keys, keyLocs, numKeys, loc);
        }
        else
            return getInvalidNode();
    }

    // Otherwise, this is a type constructor without parameters.
    return client.acceptTypeName(info, loc, qualNode);
}

Node Parser::parseModelInstantiation()
{
    // FIXME: Support the qualification of precent nodes.
    Location loc = currentLocation();
    if (reduceToken(Lexer::TKN_PERCENT))
        return client.acceptPercent(loc);

    Node qual = getNullNode();
    if (qualifierFollows()) {
        qual = parseQualifier();
        if (qual.isInvalid())
            return getInvalidNode();
    }

    return parseModelApplication(qual);
}


// Parses an "in", "out" or "in out" parameter mode specification.  If no such
// specification is available on the stream MODE_DEFAULT is returned.  A common
// mistake is to find "out in" instead of "in out".  In this case, we simply
// issue a diagnostic and return MODE_IN_OUT.
ParameterMode Parser::parseParameterMode()
{
   ParameterMode mode = MODE_DEFAULT;

    if (reduceToken(Lexer::TKN_IN)) {
        if (reduceToken(Lexer::TKN_OUT))
            mode = MODE_IN_OUT;
        else
            mode = MODE_IN;
    }
    else if (reduceToken(Lexer::TKN_OUT)) {
        if (currentTokenIs(Lexer::TKN_IN)) {
            report(diag::OUT_IN_PARAMETER_MODE);
            ignoreToken();
            mode = MODE_IN_OUT;
        }
        else
            mode = MODE_OUT;
    }
    return mode;
}

bool Parser::parseSubroutineParameter(Descriptor &desc)
{
    IdentifierInfo *formal;
    Location        location;
    ParameterMode   mode;

    location = currentLocation();
    formal   = parseIdentifierInfo();

    if (!formal) return false;

    if (!requireToken(Lexer::TKN_COLON)) return false;

    mode = parseParameterMode();
    Node type = parseModelInstantiation();
    if (type.isInvalid()) return false;

    Node param = client.acceptSubroutineParameter(formal, location, type, mode);
    if (param.isInvalid()) return false;

    desc.addParam(param);
    return true;
}

void Parser::parseSubroutineParameters(Descriptor &desc)
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
        if (!parseSubroutineParameter(desc))
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
            // The parameter list is complete.  Consume the paren and return our
            // status.
            ignoreToken();
            return;
        }
    }
}

Node Parser::parseFunctionDeclaration()
{
    Descriptor desc(&client);
    return parseFunctionDeclaration(desc);
}

Node Parser::parseFunctionDeclaration(Descriptor &desc)
{
    assert(currentTokenIs(Lexer::TKN_FUNCTION));

    desc.initialize(Descriptor::DESC_Function);
    ignoreToken();
    return parseSubroutineDeclaration(desc);
}

Node Parser::parseProcedureDeclaration()
{
    Descriptor desc(&client);
    return parseProcedureDeclaration(desc);
}

Node Parser::parseProcedureDeclaration(Descriptor &desc)
{
    assert(currentTokenIs(Lexer::TKN_PROCEDURE));

    desc.initialize(Descriptor::DESC_Procedure);
    ignoreToken();
    return parseSubroutineDeclaration(desc);
}

Node Parser::parseSubroutineDeclaration(Descriptor &desc)
{
    Location    location = currentLocation();
    IdentifierInfo *name = parseFunctionIdentifierInfo();

    if (!name) return getInvalidNode();

    desc.setIdentifier(name, location);
    client.beginSubroutineDeclaration(desc);

    if (currentTokenIs(Lexer::TKN_LPAREN))
        parseSubroutineParameters(desc);

    if (desc.isFunctionDescriptor()) {
        if (reduceToken(Lexer::TKN_RETURN)) {
            Node returnType = parseModelInstantiation();
            if (returnType.isInvalid()) {
                seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
                return getInvalidNode();
            }
            desc.setReturnType(returnType);
        }
        else {
            report(diag::MISSING_RETURN_AFTER_FUNCTION);
            return getInvalidNode();
        }
    }
    else {
        // We are parsing a procedure.  Check for "return" and post a
        // descriptive message.
        if (currentTokenIs(Lexer::TKN_RETURN)) {
            report(diag::RETURN_AFTER_PROCEDURE);
            seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
            return getInvalidNode();
        }
    }

    bool bodyFollows = currentTokenIs(Lexer::TKN_IS);
    return client.acceptSubroutineDeclaration(desc, bodyFollows);
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
    Descriptor desc(&client);
    Node       decl = parseFunctionDeclaration(desc);

    if (decl.isInvalid()) {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
        if (currentTokenIs(Lexer::TKN_IS)) {
            seekAndConsumeEndTag(desc.getIdInfo());
        }
        return;
    }

    if (reduceToken(Lexer::TKN_IS)) {
        endTagStack.push(EndTagEntry(NAMED_TAG,
                                     desc.getLocation(),
                                     desc.getIdInfo()));
        parseSubroutineBody(decl);
    }
    return;
}

void Parser::parseProcedureDeclOrDefinition()
{
    Descriptor desc(&client);
    Node       decl = parseProcedureDeclaration(desc);

    // FIXME: We should attempt to find the end of the procedure
    // decl/definition.
    if (decl.isInvalid()) return;

    if (reduceToken(Lexer::TKN_IS)) {
        endTagStack.push(EndTagEntry(NAMED_TAG,
                                     desc.getLocation(),
                                     desc.getIdInfo()));
        parseSubroutineBody(decl);
    }
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
    Location        loc;

    assert(currentTokenIs(Lexer::TKN_IDENTIFIER));

    loc = currentLocation();
    id  = parseIdentifierInfo();

    if (!(id && requireToken(Lexer::TKN_COLON))) {
        seekAndConsumeToken(Lexer::TKN_SEMI);
        return false;
    }

    Node type = parseModelInstantiation();

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
    Location location = currentLocation();

    assert(currentTokenIs(Lexer::TKN_IMPORT));
    ignoreToken();

    Node importedType = parseModelInstantiation();

    if (importedType.isValid()) {
        client.acceptImportDeclaration(importedType, location);
        return true;
    }
    return false;
}

Node Parser::parseType()
{
    assert(currentTokenIs(Lexer::TKN_TYPE));
    ignoreToken();

    Location        loc  = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name || !requireToken(Lexer::TKN_IS))
        return getInvalidNode();

    // For now, handle only enumeration types.
    if (currentTokenIs(Lexer::TKN_LPAREN)) {
        Node enumeration = client.acceptEnumerationType(name, loc);
        if (enumeration.isValid()) {
            parseEnumerationList(enumeration);
            return enumeration;
        }
        else {
            ignoreToken();
            seekCloseParen();
        }
    }
    return getInvalidNode();
}

void Parser::parseEnumerationList(Node enumeration)
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
        Location        loc  = currentLocation();
        IdentifierInfo *name = parseIdentifierInfo();

        if (!name) {
            seekCloseParen();
            return;
        }

        client.acceptEnumerationLiteral(enumeration, name, loc);
    } while (reduceToken(Lexer::TKN_COMMA));

    requireToken(Lexer::TKN_RPAREN);
}

bool Parser::parseSubroutineArgumentList(NodeVector &dst)
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    // If we have an empty set of parameters, consume them and post a
    // diagnostic.
    if (unitExprFollows()) {
        report(diag::EMPTY_PARAMS);
        ignoreToken();
        ignoreToken();
        return false;
    }

    ignoreToken();              // Ignore the '('.

    bool seenSelector = false;
    do {
        Node arg = getInvalidNode();
        if (keywordSelectionFollows()) {
            arg = parseSubroutineKeywordSelection();
            seenSelector = true;
        }
        else if (seenSelector) {
            report(diag::POSITIONAL_FOLLOWING_SELECTED_PARAMETER);
            seekCloseParen();
            return false;
        }
        else
            arg = parseExpr();

        if (arg.isInvalid()) {
            seekCloseParen();
            return false;
        }
        dst.push_back(arg);
    } while (reduceToken(Lexer::TKN_COMMA));

    if (!requireToken(Lexer::TKN_RPAREN)) {
        seekCloseParen();
        return false;
    }
    return true;
}

bool Parser::parseTopLevelDeclaration()
{
    for (;;) {
        switch (currentTokenCode()) {
        case Lexer::TKN_SIGNATURE:
        case Lexer::TKN_DOMAIN:
            parseModel();
            return true;

        case Lexer::TKN_EOT:
            return false;
            break;

        default:
            // In invalid token was found. Skip past any more garbage and try
            // again.
            report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
            seekTokens(Lexer::TKN_DOMAIN, Lexer::TKN_SIGNATURE);
        }
    }
}
