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
// As the parser proceeds, callbacks provided by the type checker are invoked.
// The parser does not build the AST explicitly -- rather, it formulates calls
// to the type checker, which in turn constructs a semanticly valid AST.
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
               Bridge         &bridge,
               Diagnostic     &diag)
    : txtProvider(txtProvider),
      idPool(idPool),
      action(bridge),
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

bool Parser::argumentSelectorFollows()
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

// Parses an end tag.  If expectedTag is non-null, parse "end <tag>;", otherwise
// parse "end;".
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
        status = requireToken(Lexer::TKN_SEMI);
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
        return action.acceptModelParameter(formal, type, loc);
    else {
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_RPAREN);
        return Node::getInvalidNode();
    }
}

// Assumes the current token is a left paren begining a model parameter list.
// Parses the list and serves it to the type checker.
void Parser::parseModelParameterization()
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
        if (node.isValid()) {
            parameters.push_back(node);
            locations.push_back(loc);
        }
    } while (reduceToken(Lexer::TKN_SEMI));

    // FIXME: We should not continue if the parameterization is invalid in any
    // way.
    action.acceptModelParameterList(&parameters[0],
                                    &locations[0], parameters.size());

    requireToken(Lexer::TKN_RPAREN);
}

// Parses a models super-signature list.  Called imediately after consuming a
// `:' as in "domain D : ...".
void Parser::parseModelSupersignatures()
{
    NodeVector supers;
    do {
        Location loc = currentLocation();
        Node super   = parseModelInstantiation();
        if (super.isValid()) {
            Node result = action.acceptModelSupersignature(super, loc);

            // If the supersignature is not valid, do not accumulate.
            if (result.isValid())
                supers.push_back(result);
            else
                action.deleteNode(result);
        }
        else {
            // We could not complete the current super signature.  Seek the next
            // super-signature, the begining of the models body, or an end tag.
            // Each of these tokens are valid grammatically.
            seekTokens(Lexer::TKN_COMMA, Lexer::TKN_WITH,
                       Lexer::TKN_ADD,   Lexer::TKN_END);
        }
    } while (reduceToken(Lexer::TKN_COMMA));
    action.acceptModelSupersignatureList(&supers[0], supers.size());
}

void Parser::parseSignatureComponents()
{
    std::vector<Node> components;

    // Currently, we only support function components.
    while (currentTokenIs(Lexer::TKN_FUNCTION)) {
        Location        location;
        IdentifierInfo *name;
        Node            type;

        location = currentLocation();
        ignoreToken();
        name = parseFunctionIdentifierInfo();

        if (!name) seekTokens(Lexer::TKN_FUNCTION,
                              Lexer::TKN_END, Lexer::TKN_ADD);

        type = parseFunctionProto();
        if (type.isInvalid())
            seekTokens(Lexer::TKN_FUNCTION,
                       Lexer::TKN_END, Lexer::TKN_ADD);

        if (requireToken(Lexer::TKN_SEMI)) {
            Node component =
                action.acceptSignatureComponent(name, type, location);
            if (component.isValid())
                components.push_back(component);
        }
    }
    action.acceptSignatureComponentList(&components[0], components.size());
}

void Parser::parseAddComponents()
{
    action.beginAddExpression();
    while (currentTokenIs(Lexer::TKN_FUNCTION))
        parseFunction();
    action.endAddExpression();
}

void Parser::parseModel()
{
    IdentifierInfo  *info;
    Location         loc;
    bool parsingDomain;

    assert(currentTokenIs(Lexer::TKN_SIGNATURE) ||
           currentTokenIs(Lexer::TKN_DOMAIN));

    parsingDomain = currentTokenIs(Lexer::TKN_DOMAIN) ? true : false;
    loc = currentLocation();
    ignoreToken();

    // If we cannot even parse the signatures name, we do not even attempt a
    // recovery sice we would not even know what end tag to look for.
    info = parseIdentifierInfo();
    if (!info) return;

    if (parsingDomain)
        action.beginDomainDefinition(info, loc);
    else
        action.beginSignatureDefinition(info, loc);

    if (currentTokenIs(Lexer::TKN_LPAREN))
        parseModelParameterization();
    else
        action.acceptModelParameterList(0, 0, 0);

    // Parse the supersignatures, if any,
    if (reduceToken(Lexer::TKN_COLON)) {
        if (!currentTokenIs(Lexer::TKN_WITH))
            parseModelSupersignatures();
        if (reduceToken(Lexer::TKN_WITH))
            parseSignatureComponents();
    }

    if (parsingDomain && reduceToken(Lexer::TKN_ADD))
        parseAddComponents();

    // Consume and verify the end tag.  On failure seek the next top level form.
    if (!parseEndTag(info))
        seekTokens(Lexer::TKN_SIGNATURE, Lexer::TKN_DOMAIN);

    action.endModelDefinition();
}

Node Parser::parseModelInstantiation()
{
    IdentifierInfo *info;
    Node            type;
    Location        loc = currentLocation();

    if (reduceToken(Lexer::TKN_PERCENT))
        return action.acceptPercent(loc);

    info = parseIdentifierInfo();
    if (!info) return type;

    if (reduceToken(Lexer::TKN_LPAREN)) {
        if (reduceToken(Lexer::TKN_RPAREN)) {
            // Empty parameter lists for types are not allowed.  If the type
            // checker accepts the non-parameterized form, then continue --
            // otherwise we complain ourselves.
            type = action.acceptTypeIdentifier(info, loc);
            if (type.isValid()) report(loc, diag::ILLEGAL_EMPTY_PARAMS);
        }
        else {
            NodeVector     arguments;
            LocationVector argumentLocs;
            IdInfoVector   selectors;
            LocationVector selectorLocs;
            bool allOK = true;

            do {
                Location loc = currentLocation();

                if (argumentSelectorFollows()) {
                    selectors.push_back(parseIdentifierInfo());
                    selectorLocs.push_back(loc);
                    ignoreToken(); // Ignore the trailing '=>'.
                    loc = currentLocation();
                }
                else if (!selectors.empty()) {
                    // We have already parsed a selector.
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
                Node            *args       = &arguments[0];
                Location        *argLocs    = &argumentLocs[0];
                IdentifierInfo **selections = &selectors[0];
                Location        *selectLocs = &selectorLocs[0];
                unsigned         arity      = arguments.size();
                unsigned         numSelects = selectors.size();
                type = action.acceptTypeApplication(
                    info, args, argLocs, arity,
                    selections, selectLocs, numSelects, loc);
            }
            else {
                // Cleanup whatever nodes we did manage to collect.
                NodeVector::iterator iter;
                NodeVector::iterator endIter = arguments.end();
                for (iter = arguments.begin(); iter != endIter; ++iter)
                    action.deleteNode(*iter);
            }
        }
    }
    else
        type = action.acceptTypeIdentifier(info, loc);

    return type;
}

bool Parser::parseFormalParameter(ParameterInfo &paramInfo, parseNodeFn parser)
{
    IdentifierInfo *formal;
    Location        formalLocation;
    Node            type;
    Location        typeLocation;

    formalLocation = currentLocation();
    formal = parseIdentifierInfo();
    if (!formal) return false;

    if (!requireToken(Lexer::TKN_COLON)) return false;

    typeLocation = currentLocation();
    type = (this->*parser)();
    if (type.isInvalid()) return false;

    paramInfo.formal         = formal;
    paramInfo.formalLocation = formalLocation;
    paramInfo.type           = type;
    paramInfo.typeLocation   = typeLocation;
    return true;
}

Node Parser::parseFunctionProto()
{
    IdInfoVector   formalIdentifiers;
    LocationVector formalLocations;
    NodeVector     parameterTypes;
    LocationVector typeLocations;
    Node           returnType;
    Location       returnLocation;
    Location       functionLocation;
    bool           allOK = true;

    if (unitExprFollows()) {
        // Simply consume the pair of parens.
        ignoreToken();
        ignoreToken();
    }
    else {
        if (requireToken(Lexer::TKN_LPAREN)) {
            // We know a parameter must follow since we did not see a unit
            // expression.
            do {
                ParameterInfo paramInfo;
                parseNodeFn   parser = &Parser::parseModelInstantiation;
                if (parseFormalParameter(paramInfo, parser)) {
                    formalIdentifiers.push_back(paramInfo.formal);
                    formalLocations.push_back(paramInfo.formalLocation);
                    parameterTypes.push_back(paramInfo.type);
                    typeLocations.push_back(paramInfo.typeLocation);
                }
                else allOK = false;
            } while (reduceToken(Lexer::TKN_SEMI));

            // If we are missing the closing paren, skip all garbage until we
            // find a 'return', then continue scanning until a terminating
            // semicolon or 'is' token is encountered.
            if (!requireToken(Lexer::TKN_RPAREN)) {
                deleteNodes(parameterTypes);
                seekAndConsumeToken(Lexer::TKN_RETURN);
                seekTokens(Lexer::TKN_IS, Lexer::TKN_SEMI);
                return Node::getInvalidNode();
            }
        }
        else {
            seekAndConsumeToken(Lexer::TKN_RETURN);
            seekTokens(Lexer::TKN_IS, Lexer::TKN_SEMI);
            return Node::getInvalidNode();
        }
    }

    // If we are missing a return, seek a terminating semicolon or 'is' keyword.
    if (!requireToken(Lexer::TKN_RETURN)) {
        deleteNodes(parameterTypes);
        seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
        return Node::getInvalidNode();
    }

    returnLocation = currentLocation();
    returnType     = parseModelInstantiation();
    if (returnType.isValid() && allOK) {
        IdentifierInfo **formals    = &formalIdentifiers[0];
        Location        *formalLocs = &formalLocations[0];
        Node            *types      = &parameterTypes[0];
        Location        *typeLocs   = &typeLocations[0];
        unsigned         arity      = formalIdentifiers.size();

        return action.acceptFunctionType(formals, formalLocs,
                                         types, typeLocs, arity,
                                         returnType, returnLocation);
    }

    // The parse failed.  Cleanup and return.
    deleteNodes(parameterTypes);
    seekTokens(Lexer::TKN_SEMI, Lexer::TKN_IS);
    return Node::getInvalidNode();
}

void Parser::parseFunction(bool allowBody)
{
    Location        location;
    IdentifierInfo *name;
    Node            type;
    Node       component;

    assert(currentTokenIs(Lexer::TKN_FUNCTION));
    location = currentLocation();
    ignoreToken();

    // The following parse can fail.  Attempt to parse the prototype regardless
    // so as to move the stream into a position where recovery is possible.
    name = parseFunctionIdentifierInfo();
    type = parseFunctionProto();

    if (!name || type.isInvalid()) {
        // parseFunctionProto attempts to accurately complete the parse, meaning
        // that we should find either EOT, a semicolon, or 'is' token.
        if (currentTokenIs(Lexer::TKN_IS))
            seekAndConsumeEndTag(name);
        return;
    }

    if (reduceToken(Lexer::TKN_IS)) {
        if (!allowBody) {
            seekAndConsumeEndTag(name);
            report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
            action.deleteNode(type);
            return;
        }

        Node function = action.beginFunctionDefinition(name, type, location);
        Node body = parseFunctionBody(name);
        action.acceptFunctionDefinition(function, body);
    }
    else if (reduceToken(Lexer::TKN_SEMI))
        action.acceptDeclaration(name, type, location);
    else {
        report(diag::UNEXPECTED_TOKEN) << currentToken().getString();
        action.deleteNode(type);

        // If another declaration is not immediately available, assume that the
        // current token is a misspelling or missing 'is' token seek the closing
        // end tag.
        if (!currentTokenIs(Lexer::TKN_FUNCTION))
            seekAndConsumeEndTag(name);
    }
}

// This parser is called just after the 'is' token begining a function
// definition.
Node Parser::parseFunctionBody(IdentifierInfo *name)
{
    if (currentTokenIs(Lexer::TKN_BEGIN))
        return parseBeginExpr(name);
    else
        return parseImplicitDeclareExpr(name);
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
