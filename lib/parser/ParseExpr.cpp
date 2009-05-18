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
    if (reduceToken(Lexer::TKN_LPAREN)) {
        Node result = parseExpr();
        if (!reduceToken(Lexer::TKN_RPAREN))
            report(diag::UNEXPECTED_TOKEN_WANTED)
                << currentTokenString() << ")";
        return result;
    }

    Node qual = getNullNode();
    if (qualifierFollows()) {
       qual = parseQualifier();
       if (qual.isInvalid())
           return getInvalidNode();
    }

    // The only primary expressions we currently support are direct names.
    Location        loc  = currentLocation();
    IdentifierInfo *name = parseIdentifierInfo();

    if (!name) {
        seekToken(Lexer::TKN_SEMI);
        return getInvalidNode();
    }

    if (currentTokenIs(Lexer::TKN_LPAREN)) {
        NodeVector args;
        if (!parseSubroutineArgumentList(args))
            return getInvalidNode();

        Node connective = client.acceptFunctionName(name, loc, qual);

        if (connective.isValid())
            return client.acceptFunctionCall(connective, loc, args);
        else
            return getInvalidNode();
    }
    else
        return client.acceptDirectName(name, loc, qual);
}
