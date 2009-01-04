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

Parser::Parser(TextProvider &tp, Bridge &bridge, Diagnostic &diag)
    : tp(tp),
      action(bridge),
      diagnostic(diag),
      lexer(tp, diag),
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

// This function scans the input tokens looking for the sequence `end <label>;'.
// If an `end' token is found, the scanning is continued in a greedy fashion and
// consumes as many matches as it encounters.  If no such sequence is found,
// this function exhausts the token stream.
bool Parser::seekEndLabel(const char *label)
{
    while (seekAndConsumeToken(Lexer::TKN_END))
    {
        IdentifierInfo *info = parseIdentifierInfo();

        if (info && strcmp(info->getString(), label) == 0) {
            reduceToken(Lexer::TKN_SEMI);
            return true;
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
    return tp.getLine(currentLocation());
}

unsigned Parser::currentColumn()
{
    return tp.getColumn(currentLocation());
}

IdentifierInfo *Parser::getIdentifierInfo(const Lexer::Token &tkn)
{
    const char *rep = tkn.getRep();
    unsigned length = tkn.getLength();
    IdentifierInfo *info = &IdentifierPool::getIdInfo(rep, length);
    return info;
}

IdentifierInfo *Parser::parseIdentifierInfo()
{
    IdentifierInfo *info;

    switch (currentTokenCode())
    {
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
        ignoreToken();
        info = 0;
    }
    return info;
}

// Parses an end tag.  If expectedTag is non-null, parse "end <tag>;", otherwise
// parse "end;".
bool Parser::parseEndTag(IdentifierInfo *expectedTag)
{
    bool status = true;
    if (status = requireToken(Lexer::TKN_END)) {
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
void Parser::parseModelParameter()
{
    IdentifierInfo *formal;
    Location loc = currentLocation();

    if ( !(formal = parseIdentifierInfo())) {
        seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
        return;
    }

    if (!requireToken(Lexer::TKN_COLON)) {
        seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
        return;
    }

    Node type = parseModelInstantiation();
    if (type.isValid())
        action.acceptModelParameter(formal, type, loc);
    else
        seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
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

    do {
        parseModelParameter();
    } while (reduceToken(Lexer::TKN_COMMA));

    requireToken(Lexer::TKN_RPAREN);
}

// Parses a models super-signature list.  Called imediately after consuming a
// `:' as in "domain D : ...".
void Parser::parseModelSupersignatures()
{
    action.beginModelSupersignatures();
    do {
        Location loc = currentLocation();
        Node super   = parseModelInstantiation();
        if (super.isValid())
            action.acceptModelSupersignature(super, loc);
        else {
            // We could not complete the current super signature.  Seek the next
            // super-signature, the begining of the models body, or an end tag.
            // Each of these tokens are valid grammatically.
            seekTokens(Lexer::TKN_COMMA, Lexer::TKN_WITH,
                       Lexer::TKN_ADD,   Lexer::TKN_END);
        }
    } while (reduceToken(Lexer::TKN_COMMA));
    action.endModelSupersignatures();
}

void Parser::parseModel()
{
    IdentifierInfo *info;
    Location loc;
    Bridge::DefinitionKind kind;

    assert(currentTokenIs(Lexer::TKN_SIGNATURE) ||
           currentTokenIs(Lexer::TKN_DOMAIN));

    // Remember what basic class of defintion we are processing.
    kind = currentTokenIs(Lexer::TKN_SIGNATURE) ? Bridge::Signature
        : Bridge::Domain;
    loc = currentLocation();
    ignoreToken();

    // If we cannot even parse the signatures name, we do not even attempt a
    // recovery sice we would not even know what end tag to look for.
    info = parseIdentifierInfo();
    if (!info) return;

    if (currentTokenIs(Lexer::TKN_LPAREN) && nextTokenIs(Lexer::TKN_RPAREN)) {
        // Empty parameter lists are not allowed. Let the type checker treat the
        // model as non-parameterized.
        report(diag::ILLEGAL_EMPTY_PARAMS);
        ignoreToken();
        ignoreToken();
        action.beginModelDefinition(kind, info, loc);
    }
    else if (currentTokenIs(Lexer::TKN_LPAREN)) {
        // We have a parameterized model.  Refine our definition kind and
        // process the parameters.
        kind = (kind == Bridge::Signature) ? Bridge::Variety : Bridge::Functor;
        action.beginModelDefinition(kind, info, loc);
        parseModelParameterization();
    }
    else {
        // A non-parameterized model.
        action.beginModelDefinition(kind, info, loc);
    }

    // Parse the supersignatures, if any,
    if (reduceToken(Lexer::TKN_COLON))
        parseModelSupersignatures();

    // Consume and verify the end tag.
    parseEndTag(info);

    action.endModelDefinition();
}

Node Parser::parseModelInstantiation()
{
    IdentifierInfo *info;
    Node type;
    Location loc = currentLocation();

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
            NodeVector arguments;
            bool allOK = true;

            do {
                Node argument = parseModelInstantiation();
                if (argument.isValid())
                    arguments.push_back(argument);
                else
                    allOK = false;
            } while (reduceToken(Lexer::TKN_COMMA));
            requireToken(Lexer::TKN_RPAREN);

            // Do not attempt to form the application unless all of the
            // arguments checked are valid.
            if (allOK) {
                Node *args = &arguments[0];
                unsigned arity = arguments.size();
                type = action.acceptTypeApplication(info, args, arity, loc);
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
