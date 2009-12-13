//===-- parser/parser.cpp ------------------------------------- -*- C++ -*-===//
//
// This file is distributed under the MIT license.  See LICENSE.txt for details.
//
// Copyright (C) 2009, Stephen Wilson
//
//===----------------------------------------------------------------------===//

#include "comma/parser/Parser.h"

#include "llvm/ADT/APInt.h"

#include <cassert>

using namespace comma;

Node Parser::parseExpr()
{
    return parseOperatorExpr();
}


Node Parser::parseOperatorExpr()
{
    return parseRelationalOperator();
}

Node Parser::parseExponentialOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parsePrimaryExpr();

    if (lhs.isInvalid())
        return getInvalidNode();

    switch (currentTokenCode()) {

    default:
        return lhs;

    case Lexer::TKN_POW:
        loc    = currentLocation();
        opInfo = parseFunctionIdentifier();
        break;
    }

    // Right associativity for the exponential operator, hense the recursive
    // call.
    Node rhs = parseExponentialOperator();

    if (rhs.isValid()) {
        Node prefix = client.acceptDirectName(opInfo, loc, false);
        if (prefix.isValid()) {
            NodeVector args;
            args.push_back(lhs);
            args.push_back(rhs);
            return client.acceptApplication(prefix, args);
        }
    }
    return getInvalidNode();
}

Node Parser::parseMultiplicativeOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parseExponentialOperator();

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_STAR:
        case Lexer::TKN_FSLASH:
        case Lexer::TKN_MOD:
        case Lexer::TKN_REM:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifier();
            break;
        }

        Node rhs = parseExponentialOperator();

        if (rhs.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptApplication(prefix, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseAdditiveOperator()
{
    Node lhs = getNullNode();

    if (currentTokenIs(Lexer::TKN_PLUS) || currentTokenIs(Lexer::TKN_MINUS)) {
        Location loc = currentLocation();
        IdentifierInfo *opInfo = parseFunctionIdentifier();

        lhs = parseMultiplicativeOperator();

        if (!lhs.isValid())
            return getInvalidNode();

        Node prefix = client.acceptDirectName(opInfo, loc, false);
        if (prefix.isValid()) {
            NodeVector args;
            args.push_back(lhs);
            lhs = client.acceptApplication(prefix, args);
        }
    }
    else
        lhs = parseMultiplicativeOperator();

    return parseBinaryAdditiveOperator(lhs);
}

Node Parser::parseBinaryAdditiveOperator(Node lhs)
{
    IdentifierInfo *opInfo;
    Location loc;

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_PLUS:
        case Lexer::TKN_MINUS:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifier();
            break;
        }

        Node rhs = parseMultiplicativeOperator();

        if (rhs.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptApplication(prefix, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseRelationalOperator()
{
    IdentifierInfo *opInfo;
    Location loc;
    Node lhs = parseAdditiveOperator();

    while (lhs.isValid()) {
        switch (currentTokenCode()) {

        default:
            return lhs;

        case Lexer::TKN_EQUAL:
        case Lexer::TKN_NEQUAL:
        case Lexer::TKN_LESS:
        case Lexer::TKN_GREAT:
        case Lexer::TKN_LEQ:
        case Lexer::TKN_GEQ:
            loc    = currentLocation();
            opInfo = parseFunctionIdentifier();
            break;
        }

        Node rhs = parseAdditiveOperator();

        if (rhs.isValid()) {
            Node prefix = client.acceptDirectName(opInfo, loc, false);
            if (prefix.isValid()) {
                NodeVector args;
                args.push_back(lhs);
                args.push_back(rhs);
                lhs = client.acceptApplication(prefix, args);
                continue;
            }
        }
        return getInvalidNode();
    }
    return lhs;
}

Node Parser::parseParenExpr()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));

    AggregateKind aggKind = aggregateFollows();

    if (aggKind == NOT_AN_AGGREGATE) {
        ignoreToken();          // Consume the opening paren.
        Node result = parseExpr();
        if (!reduceToken(Lexer::TKN_RPAREN))
            report(diag::UNEXPECTED_TOKEN_WANTED) <<
                currentTokenString() << ")";
        return result;
    }
    else
        return parseAggregate(aggKind);
}

Node Parser::parsePrimaryExpr()
{
    switch (currentTokenCode()) {

    default:
        return parseName();

    case Lexer::TKN_LPAREN:
        return parseParenExpr();

    case Lexer::TKN_INTEGER:
        return parseIntegerLiteral();

    case Lexer::TKN_STRING:
        return parseStringLiteral();
    }
}

Node Parser::parseIntegerLiteral()
{
    assert(currentTokenIs(Lexer::TKN_INTEGER));

    const char *rep = currentToken().getRep();
    unsigned repLen = currentToken().getLength();
    Location loc = ignoreToken();

    llvm::APInt value;
    decimalLiteralToAPInt(rep, repLen, value);
    return client.acceptIntegerLiteral(value, loc);
}

Node Parser::parseStringLiteral()
{
    assert(currentTokenIs(Lexer::TKN_STRING));

    const char *rep = currentToken().getRep();
    unsigned repLen = currentToken().getLength();
    Location loc = ignoreToken();

    return client.acceptStringLiteral(rep, repLen, loc);
}

Node Parser::parseAggregate(AggregateKind kind)
{
    if (kind == POSITIONAL_AGGREGATE)
        return parsePositionalAggregate();
    else if (kind == KEYED_AGGREGATE)
        return parseKeyedAggregate();

    assert(false && "Bad AggregateKind!");
    return getInvalidNode();
}

Node Parser::parseOthersExpr()
{
    assert(currentTokenIs(Lexer::TKN_OTHERS));
    ignoreToken();

    if (!requireToken(Lexer::TKN_RDARROW))
        return getInvalidNode();

    if (reduceToken(Lexer::TKN_DIAMOND))
        return getNullNode();

    return parseExpr();
}

Node Parser::parsePositionalAggregate()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    Location loc = ignoreToken();
    bool componentSeen = false;

    client.beginAggregate(loc, true);
    do {
        // If an others token is on the stream, parse the construct and
        // terminate the processing of the aggregate.
        //
        // FIXME: This code assumes the caller inspected the token stream via a
        // call to aggregateFollows().  That predicate ensures that a component
        // precedes a TKN_OTHERS token, hence the following assert.  It would be
        // much better to generate a diagnostic here rather than rely on this
        // condition.
        if (currentTokenIs(Lexer::TKN_OTHERS)) {
            assert(componentSeen);
            Location loc = currentLocation();
            Node others = parseOthersExpr();

            if (others.isValid())
                client.acceptAggregateOthers(loc, others);
            else
                seekCloseParen();
            break;
        }

        componentSeen = true;
        Node node = parseExpr();
        if (node.isValid())
            client.acceptAggregateComponent(node);
    } while (reduceToken(Lexer::TKN_COMMA));

    requireToken(Lexer::TKN_RPAREN);
    return client.endAggregate();
}


Node Parser::parseKeyedAggregate()
{
    assert(currentTokenIs(Lexer::TKN_LPAREN));
    Location loc = ignoreToken();

    client.beginAggregate(loc, false);
    do {
        // Note that "others" clauses which appear as the one and only component
        // of an aggregate litteral are categorized as keyed aggregates by the
        // grammer.
        if (currentTokenIs(Lexer::TKN_OTHERS)) {
            Location loc = currentLocation();
            Node others = parseOthersExpr();

            if (others.isValid())
                client.acceptAggregateOthers(loc, others);
            else
                seekCloseParen();
            break;
        }

        // FIXME:  The following is limited to ranges.
        Node lower = parseExpr();
        if (lower.isInvalid() || !requireToken(Lexer::TKN_DDOT)) {
            seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
            continue;
        }

        Node upper = parseExpr();
        if (upper.isInvalid() || !requireToken(Lexer::TKN_RDARROW)) {
            seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
            continue;
        }

        Node expr = parseExpr();
        if (expr.isInvalid()) {
            seekTokens(Lexer::TKN_COMMA, Lexer::TKN_RPAREN);
            continue;
        }

        client.acceptAggregateComponent(lower, upper, expr);
    } while (reduceToken(Lexer::TKN_COMMA));

    requireToken(Lexer::TKN_RPAREN);
    return client.endAggregate();
}

