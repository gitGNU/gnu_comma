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
// Tokens are requested from the lexer and stored in a small two token buffer
// which provides one token of look-ahead.  There is always a "current token",
// and each parse method begins its processing using the current token to guide
// its logic.  Therefore, the rule to follow when calling a parse method (or
// when writing one) is "the current token is the next token to be parsed".
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
      seenError(false)
{
    // Populate our small look-ahead buffer.
    lexer.scan(token[0]);
    lexer.scan(token[1]);
}

Lexer::Token &Parser::currentToken()
{
    return token[0];
}

Lexer::Token &Parser::nextToken()
{
    token[0] = token[1];
    lexer.scan(token[1]);
    return token[0];
}

Lexer::Token &Parser::peekToken()
{
    return token[1];
}

void Parser::ignoreToken()
{
    nextToken();
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

bool Parser::seekTokens(Lexer::Code code0,
                        Lexer::Code code1, Lexer::Code code2,
                        Lexer::Code code3, Lexer::Code code4)
{
    Lexer::Code codes[] = { code0, code1, code2, code3, code4 };
    Lexer::Code *end    = &codes[5];

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
            ignoreToken();
            if (depth == 0) return true;
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
            info = getIdentifierInfo(nextToken());
        }

        if (info == tag)
            return true;
    }
    return false;
}

bool Parser::seekAndConsumeEndTag(IdentifierInfo *tag)
{
    bool status = seekEndTag(tag);

    if (status) {
        ignoreToken();                // Ignore 'end'.
        ignoreToken();                // Ignore the tag.
        reduceToken(Lexer::TKN_SEMI); // Eat trailing semicolon.
    }
    return status;
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

bool Parser::keywordSelectionFollows()
{
    return currentTokenIs(Lexer::TKN_IDENTIFIER)
        && nextTokenIs(Lexer::TKN_RDARROW);
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
// parse "end".
bool Parser::parseEndTag(IdentifierInfo *expectedTag)
{
    bool status = requireToken(Lexer::TKN_END);
    if (status) {
        if (expectedTag) {
            if (currentTokenIs(Lexer::TKN_SEMI)) {
                report(diag::EXPECTED_END_TAG) << expectedTag->getString();
                status = false;
            }
            else {
                Location tagLocation = currentLocation();
                IdentifierInfo *tag  = parseIdentifierInfo();
                if (tag && tag != expectedTag) {
                    report(tagLocation, diag::EXPECTED_END_TAG)
                        << expectedTag->getString();
                    status = false;
                }
            }
        }
    }
    return status;
}

// Parses a formal parameter of a model: "id : type".  Passes the parsed type
// off to the type checker.
Node Parser::parseModelParameter()
{
    IdentifierInfo *formal;
    Location        loc;
    Node            type;

    loc = currentLocation();

    if ( !(formal = parseIdentifierInfo())) {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_RPAREN);
        return Node::getInvalidNode();
    }

    if (!requireToken(Lexer::TKN_COLON)) {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_RPAREN);
        return Node::getInvalidNode();
    }

    type = parseModelInstantiation();
    if (type.isValid())
        return client.acceptModelParameter(formal, type, loc);
    else {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_RPAREN);
        return Node::getInvalidNode();
    }
}

// Assumes the current token is a left paren begining a model parameter list.
// Parses the list and serves it to the type checker.
void Parser::parseModelParameterization(Descriptor &desc)
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    ignoreToken();

    // We do not permit empty parameter lists.
    if (currentTokenIs(Lexer::TKN_RPAREN)) {
        report(diag::ILLEGAL_EMPTY_PARAMS);
        ignoreToken();
        return;
    }

    NodeVector parameters;
    LocationVector locations;
    do {
        Location loc = currentLocation();
        Node node = parseModelParameter();
        if (node.isValid())
            desc.addParam(node);
    } while (reduceToken(Lexer::TKN_SEMI));

    requireToken(Lexer::TKN_RPAREN);
}

void Parser::parseWithExpression()
{
    assert(currentTokenIs(Lexer::TKN_WITH));
    ignoreToken();

    client.beginWithExpression();

    // Scan the set of supersignatures.  Currently this is simply a sequence of
    // signature types.  For example:
    parseWithSupersignatures();
    parseWithDeclarations();

    client.endWithExpression();
}

// Parses a sequence of super-signatures in a 'with' expression.
void Parser::parseWithSupersignatures()
{
    while (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        Location loc = currentLocation();
        Node super   = parseModelInstantiation();
        if (super.isValid()) {
            requireToken(Lexer::TKN_SEMI);
            Node result = client.acceptWithSupersignature(super, loc);
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
    Node decl;

    for (;;) {
        switch (currentTokenCode()) {
        default: return;

        case Lexer::TKN_FUNCTION:
            decl = parseFunctionDeclaration();
            break;

        case Lexer::TKN_PROCEDURE:
            decl = parseProcedureDeclaration();
            break;
        }

        if (decl.isInvalid())
            seekTokens(Lexer::TKN_FUNCTION, Lexer::TKN_PROCEDURE,
                       Lexer::TKN_SEMI, Lexer::TKN_END, Lexer::TKN_ADD);

        requireToken(Lexer::TKN_SEMI);
    }
}

void Parser::parseCarrier()
{
    IdentifierInfo *name;
    Location        loc;
    Node            type;

    assert(currentTokenIs(Lexer::TKN_CARRIER));
    ignoreToken();

    loc  = currentLocation();
    name = parseIdentifierInfo();

    if (!name) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    if (!requireToken(Lexer::TKN_IS)) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    type = parseModelInstantiation();

    if (type.isInvalid()) {
        seekToken(Lexer::TKN_SEMI);
        return;
    }

    client.acceptCarrier(name, type, loc);
}




void Parser::parseAddComponents()
{
    Descriptor desc;
    Node       component;
    client.beginAddExpression();

    for (;;) {
        switch (currentTokenCode()) {
        default:
            client.endAddExpression();
            return;

        case Lexer::TKN_FUNCTION:
            component = parseFunctionDeclaration(desc);
            if (reduceToken(Lexer::TKN_IS)) {
                endTagStack.push(EndTagEntry(NAMED_TAG,
                                             desc.getLocation(),
                                             desc.getIdInfo()));
                parseSubroutineBody(component);
            }
            break;

        case Lexer::TKN_PROCEDURE:
            component = parseProcedureDeclaration(desc);
            if (reduceToken(Lexer::TKN_IS)) {
                endTagStack.push(EndTagEntry(NAMED_TAG,
                                             desc.getLocation(),
                                             desc.getIdInfo()));
                parseSubroutineBody(component);
            }
            break;

        case Lexer::TKN_IMPORT:
            parseImportStatement();
            break;

        case Lexer::TKN_CARRIER:
            parseCarrier();
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
    Descriptor desc;

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

Node Parser::parseModelInstantiation()
{
    IdentifierInfo *info;
    Node            type;
    Location        loc = currentLocation();

    if (reduceToken(Lexer::TKN_PERCENT))
        return client.acceptPercent(loc);

    info = parseIdentifierInfo();
    if (!info) return type;

    if (reduceToken(Lexer::TKN_LPAREN)) {
        if (reduceToken(Lexer::TKN_RPAREN)) {
            // Empty parameter lists for types are not allowed.  If the type
            // checker accepts the non-parameterized form, then continue --
            // otherwise we complain ourselves.
            type = client.acceptTypeIdentifier(info, loc);
            if (type.isValid()) report(loc, diag::ILLEGAL_EMPTY_PARAMS);
        }
        else {
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
                    ignoreToken(); // Ignore the trailing '=>'.
                    loc = currentLocation();
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
                Node            *args    = &arguments[0];
                Location        *argLocs = &argumentLocs[0];
                IdentifierInfo **keys    = &keywords[0];
                Location        *keyLocs = &keywordLocs[0];
                unsigned         arity   = arguments.size();
                unsigned         numKeys = keywords.size();
                type = client.acceptTypeApplication(
                    info, args, argLocs, arity,
                    keys, keyLocs, numKeys, loc);
            }
            else {
                // Cleanup whatever nodes we did manage to collect.
                NodeVector::iterator iter;
                NodeVector::iterator endIter = arguments.end();
                for (iter = arguments.begin(); iter != endIter; ++iter)
                    client.deleteNode(*iter);
            }
        }
    }
    else
        type = client.acceptTypeIdentifier(info, loc);

    return type;
}

// Parses an "in", "out" or "in out" parameter mode specification.  If no such
// specification is available on the stream MODE_DEFAULT is returned.  A common
// mistake is to find "out int" instead of "in out".  In this case, we simply
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
    Node            type;
    Node            param;

    location = currentLocation();
    formal   = parseIdentifierInfo();

    if (!formal) return false;

    if (!requireToken(Lexer::TKN_COLON)) return false;

    mode = parseParameterMode();
    type = parseModelInstantiation();
    if (type.isInvalid()) return false;

    param = client.acceptSubroutineParameter(formal, location, type, mode);
    if (param.isInvalid()) return false;

    desc.addParam(param);
    return true;
}

void Parser::parseSubroutineParameters(Descriptor &desc)
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    // Check that we do not have an empty parameter list.
    if (unitExprFollows()) {
        report(diag::ILLEGAL_EMPTY_PARAMS);

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
    Descriptor desc;
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
    Descriptor desc;
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

    if (!name) return Node::getInvalidNode();

    desc.setIdentifier(name, location);
    client.beginSubroutineDeclaration(desc);

    if (currentTokenIs(Lexer::TKN_LPAREN)) parseSubroutineParameters(desc);

    if (desc.isFunctionDescriptor()) {
        if (reduceToken(Lexer::TKN_RETURN)) {
            Node returnType = parseModelInstantiation();
            if (returnType.isInvalid()) return Node::getInvalidNode();
            desc.setReturnType(returnType);
        }
        else {
            report(diag::MISSING_RETURN_AFTER_FUNCTION);
            return Node::getInvalidNode();
        }
    }
    else {
        // We are parsing a procedure.  Check for "return" and post a
        // descriptive message.
        if (currentTokenIs(Lexer::TKN_RETURN)) {
            report(diag::RETURN_AFTER_PROCEDURE);
            return Node::getInvalidNode();
        }
    }

    bool bodyFollows = currentTokenIs(Lexer::TKN_IS);

    return Node(client.acceptSubroutineDeclaration(desc, bodyFollows));
}

void Parser::parseSubroutineBody(Node declarationNode)
{
    client.beginSubroutineDefinition(declarationNode);

    while (currentTokenIs(Lexer::TKN_IDENTIFIER) ||
           currentTokenIs(Lexer::TKN_FUNCTION))
        parseDeclaration();

    requireToken(Lexer::TKN_BEGIN);

    while (!(currentTokenIs(Lexer::TKN_END) ||
             currentTokenIs(Lexer::TKN_EOT))) {
        parseStatement();
        requireToken(Lexer::TKN_SEMI);
    }

    EndTagEntry tagEntry = endTagStack.top();
    assert(tagEntry.kind == NAMED_TAG && "Inconsistent end tag stack!");

    endTagStack.pop();
    parseEndTag(tagEntry.tag);
    client.endSubroutineDefinition();
}

Node Parser::parseDeclaration()
{
    switch (currentTokenCode()) {
    default:
        report(diag::UNEXPECTED_TOKEN) << currentTokenString();
        return Node::getInvalidNode();

    case Lexer::TKN_IDENTIFIER:
        return parseObjectDeclaration();

    case Lexer::TKN_FUNCTION:
        return parseFunctionDeclaration();

    case Lexer::TKN_PROCEDURE:
        return parseProcedureDeclaration();
    }
}

Node Parser::parseObjectDeclaration()
{
    IdentifierInfo *id;
    Node            type;
    Location        loc;
    Node            init;

    assert(currentTokenIs(Lexer::TKN_IDENTIFIER));

    loc = currentLocation();
    id  = parseIdentifierInfo();

    if (!(id && requireToken(Lexer::TKN_COLON))) {
        seekAndConsumeToken(Lexer::TKN_SEMI);
        return Node::getInvalidNode();
    }

    type = parseModelInstantiation();

    if (type.isValid())
        switch (currentTokenCode()) {
        default:
            report(diag::UNEXPECTED_TOKEN) << currentTokenString();
            return Node::getInvalidNode();

        case Lexer::TKN_SEMI:
            ignoreToken();
            return client.acceptDeclaration(id, type, loc);

        case Lexer::TKN_ASSIGN:
            ignoreToken();
            init = parseExpr();
            if (init.isValid()) {
                Node result = client.acceptDeclaration(id, type, loc);
                client.acceptDeclarationInitializer(result, init);
                requireToken(Lexer::TKN_SEMI);
                return result;
            }
            else
                seekAndConsumeToken(Lexer::TKN_SEMI);
            return Node::getInvalidNode();
        }
    else {
        seekAndConsumeToken(Lexer::TKN_SEMI);
        return Node::getInvalidNode();
    }
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
