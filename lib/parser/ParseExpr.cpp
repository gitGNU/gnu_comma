//===-- parser/parser.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"
#include <cassert>

using namespace comma;

Node Parser::parseExpr()
{
    switch (currentTokenCode()) {

    default:
        return parsePrimaryExpr();

    case Lexer::TKN_INJ:
        return parseInjExpr();

    case Lexer::TKN_PRJ:
        return parsePrjExpr();
    }
}

Node Parser::parseSubroutineKeywordSelection()
{
    assert(keywordSelectionFollows());
    Location        loc  = currentLocation();
    IdentifierInfo *key  = parseIdentifierInfo();

    ignoreToken();              // consume the "=>".
    Node expr = parseExpr();

    if (expr.isInvalid())
        return getInvalidNode();
    else
        return client.acceptKeywordSelector(key, loc, expr, true);
}

Node Parser::parseQualificationExpr()
{
    Location location  = currentLocation();
    Node qualifierType = parseModelInstantiation();

    if (qualifierType.isInvalid()) {
        do {
            seekAndConsumeToken(Lexer::TKN_DCOLON);
        } while (qualificationFollows());
        return getInvalidNode();
    }

    if (reduceToken(Lexer::TKN_DCOLON)) {

        Node qualifier = client.acceptQualifier(qualifierType, location);

        while (qualificationFollows()) {
            location      = currentLocation();
            qualifierType = parseModelInstantiation();

            if (qualifierType.isInvalid()) {
                do {
                    seekAndConsumeToken(Lexer::TKN_DCOLON);
                } while (qualificationFollows());
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

Node Parser::parseInjExpr()
{
    assert(currentTokenIs(Lexer::TKN_INJ));

    Location loc  = ignoreToken();
    Node     expr = parseExpr();

    if (expr.isValid())
        return client.acceptInj(loc, expr);
    return getInvalidNode();
}

Node Parser::parsePrjExpr()
{
    assert(currentTokenIs(Lexer::TKN_PRJ));

    Location loc  = ignoreToken();
    Node     expr = parseExpr();

    if (expr.isValid())
        return client.acceptPrj(loc, expr);
    return getInvalidNode();
}

Node Parser::parsePrimaryExpr()
{
    Node     qualifier = Node::getNullNode(&client);
    Location loc       = currentLocation();

    if (reduceToken(Lexer::TKN_LPAREN)) {
        Node result = parseExpr();
        if (!reduceToken(Lexer::TKN_RPAREN))
            report(diag::UNEXPECTED_TOKEN_WANTED)
                << currentTokenString() << ")";
        return result;
    }

    // FIXME:  Use result of qualification parsing.
    if (qualificationFollows()) {
       qualifier = parseQualificationExpr();
       if (qualifier.isInvalid())
           return getInvalidNode();
    }

    // The only primary expressions we currently support are direct names.
    IdentifierInfo *directName = parseIdentifierInfo();

    if (!directName) {
        seekToken(Lexer::TKN_SEMI);
        return getInvalidNode();
    }

    // If we have an empty set of parameters, treat as a nullary name.
    if (unitExprFollows()) {
        report(diag::ILLEGAL_EMPTY_PARAMS);
        ignoreToken();
        ignoreToken();

        if (qualifier.isNull())
            return client.acceptDirectName(directName, loc);
        else
            return client.acceptQualifiedName(qualifier, directName, loc);
    }

    if (reduceToken(Lexer::TKN_LPAREN)) {
        NodeVector arguments;
        bool       seenSelector = false;
        do {
            Node arg = getInvalidNode();
            if (keywordSelectionFollows()) {
                arg = parseSubroutineKeywordSelection();
                seenSelector = true;
            }
            else if (seenSelector) {
                report(diag::POSITIONAL_FOLLOWING_SELECTED_PARAMETER);
                seekCloseParen();
                return getInvalidNode();
            }
            else
                arg = parseExpr();

            if (arg.isInvalid()) {
                seekCloseParen();
                return getInvalidNode();
            }
            arguments.push_back(arg);
        } while (reduceToken(Lexer::TKN_COMMA));

        if (!requireToken(Lexer::TKN_RPAREN)) {
            seekCloseParen();
            return getInvalidNode();
        }

        return client.acceptFunctionCall(directName, loc, arguments);
    }

    if (qualifier.isNull())
        return client.acceptDirectName(directName, loc);
    else
        return client.acceptQualifiedName(qualifier, directName, loc);
}
