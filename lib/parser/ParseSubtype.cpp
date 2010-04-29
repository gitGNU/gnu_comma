//===-- parser/ParseSubtype.cpp ------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license. See LICENSE.txt for details.
//
// Copyright (C) 2010, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"

using namespace comma;

bool Parser::parseRange(Node &lower, Node &upper)
{
    lower = parseExpr();
    if (lower.isInvalid() || !requireToken(Lexer::TKN_DDOT))
        return false;

    upper = parseExpr();
    if (upper.isInvalid())
        return false;

    return true;
}

Node Parser::parseSubtypeIndicationRange(Node subtypeMark)
{
    // FIXME: Have parseDSTDefinition call this method.
    assert(currentTokenIs(Lexer::TKN_RANGE));
    ignoreToken();

    Node lower = getNullNode();
    Node upper = getNullNode();
    if (!parseRange(lower, upper))
        return getInvalidNode();

    return client.acceptSubtypeIndication(subtypeMark, lower, upper);
}

Node Parser::parseSubtypeIndicationArgument()
{

    // FIXME: This parse logic needs to be extended to support the discriminant
    // selector syntax "A | B => ...".  The following handles only one selector
    // name on the lhs of the arrow.
    if (keywordSelectionFollows())
        return parseParameterAssociation();

    // Parse an expression.
    //
    // FIXME: We need to accept range attributes here as well.  Speculative
    // parsing seems in order.
    Node expr = parseExpr();
    if (!expr.isValid())
        return getInvalidNode();

    // If we have a "..", form a DST definition.
    if (reduceToken(Lexer::TKN_DDOT)) {
        Node upper = parseExpr();

        if (upper.isInvalid())
            return getInvalidNode();

        return client.acceptDSTDefinition(expr, upper);
    }

    // Again, if we have a range form a DST definition.
    if (reduceToken(Lexer::TKN_RANGE)) {
        Node lower = getNullNode();
        Node upper = getNullNode();

        if (parseRange(lower, upper))
            return client.acceptDSTDefinition(expr, lower, upper);
        else
            return getInvalidNode();
    }

    // Otherwise, the parsed expression must stand on its own.
    return expr;
}

Node Parser::parseSubtypeIndicationArguments(Node subtypeMark)
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    ignoreToken();

    NodeVector params;
    do {
        Node param = parseSubtypeIndicationArgument();

        if (param.isInvalid()) {
            seekCloseParen();
            return getInvalidNode();
        }

        params.push_back(param);
    } while (reduceToken(Lexer::TKN_COMMA));

    // Be permissive.  If we do not have a close paren just seek till we find
    // one.  Let the client decide the validity of the construct.
    if (!requireToken(Lexer::TKN_RPAREN))
        seekCloseParen();

    return client.acceptSubtypeIndication(subtypeMark, params);
}

Node Parser::parseSubtypeIndication()
{
    Node subtypeMark = parseName(Elide_Application);

    if (subtypeMark.isInvalid())
        return getInvalidNode();

    switch (currentTokenCode()) {

    default:
        return client.acceptSubtypeIndication(subtypeMark);

    case Lexer::TKN_RANGE:
        return parseSubtypeIndicationRange(subtypeMark);

    case Lexer::TKN_LPAREN:
        return parseSubtypeIndicationArguments(subtypeMark);
    }
}


