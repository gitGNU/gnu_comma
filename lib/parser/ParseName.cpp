//===-- parser/ParseName.cpp ---------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"

using namespace comma;

Node Parser::parseDirectName(bool forStatement)
{
    Location loc = currentLocation();

    if (currentTokenIs(Lexer::TKN_CHARACTER)) {
        IdentifierInfo *name = parseCharacter();
        if (name)
            return client.acceptCharacterLiteral(name, loc);
    }

    if (currentTokenIs(Lexer::TKN_IDENTIFIER)) {
        IdentifierInfo *name = parseIdentifierInfo();
        if (name)
            return client.acceptDirectName(name, loc, forStatement);
    }

    if (reduceToken(Lexer::TKN_PERCENT))
        return client.acceptPercent(loc);

    report(diag::UNEXPECTED_TOKEN) << currentTokenString();
    seekNameEnd();
    return getInvalidNode();
}

Node Parser::parseSelectedComponent(Node prefix, bool forStatement)
{
    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierOrCharacter();

    if (name)
        return client.acceptSelectedComponent(prefix, name, loc, forStatement);

    seekNameEnd();
    return getInvalidNode();
}

Node Parser::parseApplication(Node prefix)
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    if (unitExprFollows()) {
        Location loc = ignoreToken(); // Ignore the opening paren;
        ignoreToken();                // Ingore the close paren;
        Node result = client.finishName(prefix);
        if (result.isValid())
            report(loc, diag::EMPTY_PARAMS);
        return result;
    }

    ignoreToken();              // Ignore the opening paren.
    NodeVector arguments;
    bool seenSelector = false;

    do {
        Node arg = getInvalidNode();
        if (keywordSelectionFollows()) {
            arg = parseParameterAssociation();
            seenSelector = true;
        }
        else if (seenSelector) {
            report(diag::POSITIONAL_FOLLOWING_SELECTED_PARAMETER);
            seekCloseParen();
        }
        else
            arg = parseExpr();

        if (arg.isValid())
            arguments.push_back(arg);
        else {
            seekCloseParen();
            return getInvalidNode();
        }
    } while (reduceToken(Lexer::TKN_COMMA));

    if (!requireToken(Lexer::TKN_RPAREN)) {
        seekCloseParen();
        return getInvalidNode();
    }
    return client.acceptApplication(prefix, arguments);
}

Node Parser::parseParameterAssociation()
{
    assert(keywordSelectionFollows());

    Location loc = currentLocation();
    IdentifierInfo *key = parseIdentifierInfo();

    ignoreToken();              // Ignore the =>.

    Node rhs = parseExpr();

    if (rhs.isValid())
        return client.acceptParameterAssociation(key, loc, rhs);
    else
        return getInvalidNode();
}

Node Parser::parseAttribute(Node prefix)
{
    Location loc = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (name->getAttributeID() == attrib::UNKNOWN_ATTRIBUTE) {
        report(loc, diag::UNKNOWN_ATTRIBUTE) << name;
        return getInvalidNode();
    }

    /// Names can be parsed in a variety of contexts.  However, Range attributes
    /// are a special case as there are special parse rules which consume them.
    if (name->getAttributeID() == attrib::Range) {
        report(loc, diag::INVALID_ATTRIBUTE_CONTEXT) << name;
        return getInvalidNode();
    }

    return client.acceptAttribute(prefix, name, loc);
}

Node Parser::parseName(bool forStatement)
{
    Location loc = currentLocation();

    // All names start with a direct name.
    Node prefix = parseDirectName(forStatement);

    if (prefix.isInvalid())
        return prefix;

    for ( ;; ) {
        if (currentTokenIs(Lexer::TKN_LPAREN))
            prefix = parseApplication(prefix);
        else if (reduceToken(Lexer::TKN_DOT)) {
            prefix = client.finishName(prefix);
            if (prefix.isValid())
                prefix = parseSelectedComponent(prefix, forStatement);
        }
        else if (reduceToken(Lexer::TKN_QUOTE)) {
            prefix = client.finishName(prefix);
            if (prefix.isValid())
                prefix = parseAttribute(prefix);
        }
        else
            break;

        if (prefix.isInvalid())
            break;
    }

    if (prefix.isInvalid())
        return prefix;
    else
        return client.finishName(prefix);
}

void Parser::seekNameEnd()
{
    for ( ;; ) {
        switch(currentTokenCode()) {

        default:
            return;

        case Lexer::TKN_IDENTIFIER:
        case Lexer::TKN_DOT:
        case Lexer::TKN_CHARACTER:
        case Lexer::TKN_PERCENT:
            ignoreToken();
            break;

        case Lexer::TKN_LPAREN:
            ignoreToken();
            seekCloseParen();
        };
    }
}
